/*
 * MTA:SA Android - JNI Bridge Implementation
 */

#include "MTANative.h"
#include "../platform/AndroidInput.h"
#include "../platform/AndroidFileSystem.h"
#include "../platform/AndroidNetwork.h"

#include <cstring>
#include <cstdlib>

#ifdef __ANDROID__
#include <android/log.h>
#include <android/asset_manager_jni.h>
#define LOG_TAG "MTA-JNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#include <cstdio>
#define LOGI(...) printf(__VA_ARGS__)
#define LOGD(...) printf(__VA_ARGS__)
#define LOGE(...) fprintf(stderr, __VA_ARGS__)
#endif

using namespace MTA::Android::Platform;

// Forward declarations from MTAAndroidMain.cpp
namespace MTA::Android {
    bool Initialize();
    void Shutdown();
}

//=============================================================================
// Global State
//=============================================================================

static JavaVM* g_JavaVM = nullptr;
static bool g_Initialized = false;
static int g_ScreenWidth = 0;
static int g_ScreenHeight = 0;
static bool g_Paused = false;

// Connection state (local enum to avoid conflict with Platform::ConnectionState)
enum class JNIConnectionState { Disconnected = 0, Connecting = 1, Connected = 2 };
static JNIConnectionState g_ConnectionState = JNIConnectionState::Disconnected;

//=============================================================================
// JNI Library Entry Points
//=============================================================================

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved)
{
    LOGI("MTA:SA Android native library loaded");

    g_JavaVM = vm;

    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK)
    {
        LOGE("Failed to get JNI environment");
        return JNI_ERR;
    }

    // Initialize MTA core
    MTA::Android::Initialize();

    return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved)
{
    LOGI("MTA:SA Android native library unloading");

    if (g_Initialized)
    {
        // Shutdown subsystems
        AndroidInput::Instance().Shutdown();
        AndroidFileSystem::Instance().Shutdown();
        AndroidNetwork::Instance().Shutdown();

        g_Initialized = false;
    }

    // Shutdown MTA core
    MTA::Android::Shutdown();

    g_JavaVM = nullptr;
}

//=============================================================================
// Core MTA Functions
//=============================================================================

JNIEXPORT jboolean JNICALL Java_com_mtasa_android_MTANative_nativeInit(
    JNIEnv* env, jobject obj, jobject context, jobject assetManager)
{
    LOGI("Initializing MTA:SA Android...");

    if (g_Initialized)
    {
        LOGI("Already initialized");
        return JNI_TRUE;
    }

#ifdef __ANDROID__
    // Get asset manager
    AAssetManager* assets = AAssetManager_fromJava(env, assetManager);
    if (!assets)
    {
        LOGE("Failed to get asset manager");
        return JNI_FALSE;
    }

    // Initialize file system
    if (!AndroidFileSystem::Instance().Initialize(env, context, assets))
    {
        LOGE("Failed to initialize file system");
        return JNI_FALSE;
    }

    // Initialize network
    if (!AndroidNetwork::Instance().Initialize(env, context))
    {
        LOGE("Failed to initialize network");
        return JNI_FALSE;
    }

    // Initialize input (screen size will be set later)
    if (!AndroidInput::Instance().Initialize(1920, 1080))
    {
        LOGE("Failed to initialize input");
        return JNI_FALSE;
    }

    // Initialize Java bridge
    MTA::Android::JNI::JavaBridge::Instance().Initialize(env, context);
#endif

    g_Initialized = true;
    LOGI("MTA:SA Android initialized successfully");

    return JNI_TRUE;
}

JNIEXPORT void JNICALL Java_com_mtasa_android_MTANative_nativeShutdown(
    JNIEnv* env, jobject obj)
{
    LOGI("Shutting down MTA:SA Android...");

    if (!g_Initialized)
        return;

    // Shutdown subsystems in reverse order
    MTA::Android::JNI::JavaBridge::Instance().Shutdown();
    AndroidInput::Instance().Shutdown();
    AndroidNetwork::Instance().Shutdown();
    AndroidFileSystem::Instance().Shutdown();

    g_Initialized = false;
    LOGI("MTA:SA Android shutdown complete");
}

JNIEXPORT void JNICALL Java_com_mtasa_android_MTANative_nativeUpdate(
    JNIEnv* env, jobject obj, jfloat deltaTime)
{
    if (!g_Initialized || g_Paused)
        return;

    // Update input system
    AndroidInput::Instance().Update(deltaTime);

    // Update game logic
    // TODO: Call into game_sa/multiplayer modules
}

JNIEXPORT void JNICALL Java_com_mtasa_android_MTANative_nativeRender(
    JNIEnv* env, jobject obj)
{
    if (!g_Initialized || g_Paused)
        return;

    // Render frame
    // TODO: Call into graphics module
}

JNIEXPORT void JNICALL Java_com_mtasa_android_MTANative_nativeSurfaceChanged(
    JNIEnv* env, jobject obj, jint width, jint height)
{
    LOGI("Surface changed: %dx%d", width, height);

    g_ScreenWidth = width;
    g_ScreenHeight = height;

    // Update input system with new screen size
    AndroidInput::Instance().SetScreenSize(width, height);

    // Update graphics viewport
    // TODO: Call into graphics module
}

JNIEXPORT void JNICALL Java_com_mtasa_android_MTANative_nativeSurfaceDestroyed(
    JNIEnv* env, jobject obj)
{
    LOGI("Surface destroyed");

    // Release graphics resources
    // TODO: Call into graphics module
}

JNIEXPORT void JNICALL Java_com_mtasa_android_MTANative_nativePause(
    JNIEnv* env, jobject obj)
{
    LOGI("App paused");
    g_Paused = true;

    // Pause audio, etc.
}

JNIEXPORT void JNICALL Java_com_mtasa_android_MTANative_nativeResume(
    JNIEnv* env, jobject obj)
{
    LOGI("App resumed");
    g_Paused = false;

    // Resume audio, etc.
}

//=============================================================================
// Input Events
//=============================================================================

JNIEXPORT void JNICALL Java_com_mtasa_android_MTANative_nativeTouchEvent(
    JNIEnv* env, jobject obj, jint action, jint pointerId,
    jfloat x, jfloat y, jfloat pressure)
{
    AndroidInput::Instance().OnTouchEvent(action, pointerId, x, y, pressure);
}

JNIEXPORT void JNICALL Java_com_mtasa_android_MTANative_nativeKeyEvent(
    JNIEnv* env, jobject obj, jint keyCode, jint action, jint metaState)
{
    AndroidInput::Instance().OnKeyEvent(keyCode, action, metaState);
}

JNIEXPORT void JNICALL Java_com_mtasa_android_MTANative_nativeGamepadConnected(
    JNIEnv* env, jobject obj, jint deviceId, jstring name)
{
    const char* nameStr = env->GetStringUTFChars(name, nullptr);
    AndroidInput::Instance().OnGamepadConnected(deviceId, nameStr);
    env->ReleaseStringUTFChars(name, nameStr);
}

JNIEXPORT void JNICALL Java_com_mtasa_android_MTANative_nativeGamepadDisconnected(
    JNIEnv* env, jobject obj, jint deviceId)
{
    AndroidInput::Instance().OnGamepadDisconnected(deviceId);
}

JNIEXPORT void JNICALL Java_com_mtasa_android_MTANative_nativeGamepadButton(
    JNIEnv* env, jobject obj, jint deviceId, jint button, jboolean pressed)
{
    AndroidInput::Instance().OnGamepadButton(deviceId, button, pressed == JNI_TRUE);
}

JNIEXPORT void JNICALL Java_com_mtasa_android_MTANative_nativeGamepadAxis(
    JNIEnv* env, jobject obj, jint deviceId, jint axis, jfloat value)
{
    AndroidInput::Instance().OnGamepadAxis(deviceId, axis, value);
}

//=============================================================================
// Network Events
//=============================================================================

JNIEXPORT void JNICALL Java_com_mtasa_android_MTANative_nativeNetworkStateChanged(
    JNIEnv* env, jobject obj, jboolean available, jint type, jboolean metered)
{
    AndroidNetwork::Instance().OnNetworkStateChanged(
        available == JNI_TRUE, type, metered == JNI_TRUE);
}

//=============================================================================
// Server Connection
//=============================================================================

JNIEXPORT jboolean JNICALL Java_com_mtasa_android_MTANative_nativeConnect(
    JNIEnv* env, jobject obj, jstring host, jint port,
    jstring nick, jstring password)
{
    const char* hostStr = env->GetStringUTFChars(host, nullptr);
    const char* nickStr = env->GetStringUTFChars(nick, nullptr);
    const char* passStr = env->GetStringUTFChars(password, nullptr);

    LOGI("Connecting to %s:%d as %s", hostStr, port, nickStr);

    // TODO: Implement actual connection logic
    g_ConnectionState = JNIConnectionState::Connecting;

    env->ReleaseStringUTFChars(host, hostStr);
    env->ReleaseStringUTFChars(nick, nickStr);
    env->ReleaseStringUTFChars(password, passStr);

    return JNI_TRUE;
}

JNIEXPORT void JNICALL Java_com_mtasa_android_MTANative_nativeDisconnect(
    JNIEnv* env, jobject obj)
{
    LOGI("Disconnecting");

    // TODO: Implement actual disconnection logic
    g_ConnectionState = JNIConnectionState::Disconnected;
}

JNIEXPORT jint JNICALL Java_com_mtasa_android_MTANative_nativeGetConnectionState(
    JNIEnv* env, jobject obj)
{
    return static_cast<jint>(g_ConnectionState);
}

//=============================================================================
// Configuration
//=============================================================================

JNIEXPORT void JNICALL Java_com_mtasa_android_MTANative_nativeSetGameDataPath(
    JNIEnv* env, jobject obj, jstring path)
{
    const char* pathStr = env->GetStringUTFChars(path, nullptr);
    AndroidFileSystem::Instance().SetGameDataPath(pathStr);
    env->ReleaseStringUTFChars(path, pathStr);
}

JNIEXPORT jstring JNICALL Java_com_mtasa_android_MTANative_nativeGetVersion(
    JNIEnv* env, jobject obj)
{
    return env->NewStringUTF("1.6.0-android");
}

JNIEXPORT jstring JNICALL Java_com_mtasa_android_MTANative_nativeGetBuildDate(
    JNIEnv* env, jobject obj)
{
    return env->NewStringUTF(__DATE__ " " __TIME__);
}

//=============================================================================
// Simplified API (static methods)
//=============================================================================

JNIEXPORT jboolean JNICALL Java_com_mtasa_android_MTANative_initialize(
    JNIEnv* env, jclass clazz)
{
    return MTA::Android::Initialize() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_com_mtasa_android_MTANative_isInitialized(
    JNIEnv* env, jclass clazz)
{
    return g_Initialized ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL Java_com_mtasa_android_MTANative_getVersion(
    JNIEnv* env, jclass clazz)
{
    return env->NewStringUTF("MTA:SA Android 1.6.0-alpha");
}

JNIEXPORT jstring JNICALL Java_com_mtasa_android_MTANative_getAddressMappings(
    JNIEnv* env, jclass clazz)
{
    // Get from signature scanner if available
    return env->NewStringUTF("{}");  // Placeholder
}

//=============================================================================
// GTA:SA Integration (Phase 6)
//=============================================================================

// Include integration header
#include "../game_sa/GTASAIntegration.h"

JNIEXPORT jboolean JNICALL Java_com_mtasa_android_MTANative_initGTASAIntegration(
    JNIEnv* env, jclass clazz)
{
    LOGI("Initializing GTA:SA integration from JNI with God Mode...");
    bool result = MTA::Android::GTASA::InitializeWithGodMode();

    if (result)
    {
        // Show toast notification via Java
        MTA::Android::JNI::JavaBridge::Instance().ShowToast("MTA:SA Loaded - God Mode ON!");
    }

    return result ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL Java_com_mtasa_android_MTANative_getIntegrationStatus(
    JNIEnv* env, jclass clazz)
{
    std::string status = MTA::Android::GTASA::GetStatusJSON();
    return env->NewStringUTF(status.c_str());
}

JNIEXPORT jboolean JNICALL Java_com_mtasa_android_MTANative_enableGodMode(
    JNIEnv* env, jclass clazz)
{
    return MTA::Android::GTASA::EnableGodMode() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL Java_com_mtasa_android_MTANative_disableGodMode(
    JNIEnv* env, jclass clazz)
{
    MTA::Android::GTASA::DisableGodMode();
}

JNIEXPORT jboolean JNICALL Java_com_mtasa_android_MTANative_toggleGodMode(
    JNIEnv* env, jclass clazz)
{
    return MTA::Android::GTASA::ToggleGodMode() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_com_mtasa_android_MTANative_isGodModeEnabled(
    JNIEnv* env, jclass clazz)
{
    return MTA::Android::GTASA::GetState().godModeEnabled ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL Java_com_mtasa_android_MTANative_getGTASAVersion(
    JNIEnv* env, jclass clazz)
{
    const char* version = MTA::Android::GTASA::GetVersionString(
        MTA::Android::GTASA::GetState().gameLib.version);
    return env->NewStringUTF(version);
}

JNIEXPORT jboolean JNICALL Java_com_mtasa_android_MTANative_isGameLibraryReady(
    JNIEnv* env, jclass clazz)
{
    return MTA::Android::GTASA::IsReady() ? JNI_TRUE : JNI_FALSE;
}

//=============================================================================
// JavaBridge Implementation
//=============================================================================

namespace MTA::Android::JNI
{

JavaBridge& JavaBridge::Instance()
{
    static JavaBridge instance;
    return instance;
}

JavaBridge::JavaBridge()
    : m_javaVM(nullptr)
    , m_activity(nullptr)
    , m_bridgeClass(nullptr)
    , m_showToast(nullptr)
    , m_showDialog(nullptr)
    , m_requestPermission(nullptr)
    , m_openURL(nullptr)
    , m_setClipboard(nullptr)
    , m_getClipboard(nullptr)
    , m_updateProgress(nullptr)
    , m_showKeyboard(nullptr)
    , m_vibrate(nullptr)
{
}

JavaBridge::~JavaBridge()
{
    Shutdown();
}

bool JavaBridge::Initialize(JNIEnv* env, jobject activity)
{
    LOGI("Initializing Java bridge...");

    // Get JavaVM
    env->GetJavaVM(&m_javaVM);

    // Create global reference to activity
    m_activity = env->NewGlobalRef(activity);

    // Find bridge class
    jclass bridgeClass = env->FindClass("com/mtasa/android/MTABridge");
    if (bridgeClass)
    {
        m_bridgeClass = (jclass)env->NewGlobalRef(bridgeClass);
        env->DeleteLocalRef(bridgeClass);

        // Get method IDs
        m_showToast = env->GetStaticMethodID(m_bridgeClass, "showToast",
            "(Landroid/content/Context;Ljava/lang/String;)V");
        m_showDialog = env->GetStaticMethodID(m_bridgeClass, "showDialog",
            "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;)V");
        m_requestPermission = env->GetStaticMethodID(m_bridgeClass, "requestPermission",
            "(Landroid/app/Activity;Ljava/lang/String;)V");
        m_openURL = env->GetStaticMethodID(m_bridgeClass, "openURL",
            "(Landroid/content/Context;Ljava/lang/String;)V");
        m_showKeyboard = env->GetStaticMethodID(m_bridgeClass, "showKeyboard",
            "(Landroid/content/Context;Z)V");
        m_vibrate = env->GetStaticMethodID(m_bridgeClass, "vibrate",
            "(Landroid/content/Context;I)V");

        LOGI("Java bridge initialized");
    }
    else
    {
        LOGE("Could not find MTABridge class");
        env->ExceptionClear();
    }

    return true;
}

void JavaBridge::Shutdown()
{
    if (!m_javaVM)
        return;

    JNIEnv* env = GetEnv();
    if (env)
    {
        if (m_activity)
        {
            env->DeleteGlobalRef(m_activity);
            m_activity = nullptr;
        }
        if (m_bridgeClass)
        {
            env->DeleteGlobalRef(m_bridgeClass);
            m_bridgeClass = nullptr;
        }
    }

    m_javaVM = nullptr;
}

JNIEnv* JavaBridge::GetEnv()
{
    if (!m_javaVM)
        return nullptr;

    JNIEnv* env;
    int status = m_javaVM->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);

    if (status == JNI_EDETACHED)
    {
        // Attach this thread to the VM
        if (m_javaVM->AttachCurrentThread(&env, nullptr) != 0)
        {
            LOGE("Failed to attach thread to JVM");
            return nullptr;
        }
    }
    else if (status != JNI_OK)
    {
        return nullptr;
    }

    return env;
}

void JavaBridge::ShowToast(const char* message)
{
    JNIEnv* env = GetEnv();
    if (!env || !m_bridgeClass || !m_showToast || !m_activity)
        return;

    jstring msg = env->NewStringUTF(message);
    env->CallStaticVoidMethod(m_bridgeClass, m_showToast, m_activity, msg);
    env->DeleteLocalRef(msg);
}

void JavaBridge::ShowDialog(const char* title, const char* message)
{
    JNIEnv* env = GetEnv();
    if (!env || !m_bridgeClass || !m_showDialog || !m_activity)
        return;

    jstring titleStr = env->NewStringUTF(title);
    jstring msgStr = env->NewStringUTF(message);
    env->CallStaticVoidMethod(m_bridgeClass, m_showDialog, m_activity, titleStr, msgStr);
    env->DeleteLocalRef(titleStr);
    env->DeleteLocalRef(msgStr);
}

void JavaBridge::RequestPermission(const char* permission)
{
    JNIEnv* env = GetEnv();
    if (!env || !m_bridgeClass || !m_requestPermission || !m_activity)
        return;

    jstring perm = env->NewStringUTF(permission);
    env->CallStaticVoidMethod(m_bridgeClass, m_requestPermission, m_activity, perm);
    env->DeleteLocalRef(perm);
}

void JavaBridge::OpenURL(const char* url)
{
    JNIEnv* env = GetEnv();
    if (!env || !m_bridgeClass || !m_openURL || !m_activity)
        return;

    jstring urlStr = env->NewStringUTF(url);
    env->CallStaticVoidMethod(m_bridgeClass, m_openURL, m_activity, urlStr);
    env->DeleteLocalRef(urlStr);
}

void JavaBridge::SetClipboardText(const char* text)
{
    // TODO: Implement
}

const char* JavaBridge::GetClipboardText()
{
    // TODO: Implement
    return "";
}

void JavaBridge::UpdateLoadingProgress(int percent, const char* status)
{
    // TODO: Implement
}

void JavaBridge::ShowServerBrowser()
{
    // TODO: Implement
}

void JavaBridge::HideServerBrowser()
{
    // TODO: Implement
}

void JavaBridge::ShowKeyboard(bool show)
{
    JNIEnv* env = GetEnv();
    if (!env || !m_bridgeClass || !m_showKeyboard || !m_activity)
        return;

    env->CallStaticVoidMethod(m_bridgeClass, m_showKeyboard, m_activity, show ? JNI_TRUE : JNI_FALSE);
}

bool JavaBridge::IsKeyboardVisible()
{
    // TODO: Implement
    return false;
}

void JavaBridge::Vibrate(int milliseconds)
{
    JNIEnv* env = GetEnv();
    if (!env || !m_bridgeClass || !m_vibrate || !m_activity)
        return;

    env->CallStaticVoidMethod(m_bridgeClass, m_vibrate, m_activity, milliseconds);
}

//=============================================================================
// Utility Functions
//=============================================================================

JNIEnv* GetJNIEnv()
{
    return JavaBridge::Instance().GetEnv();
}

char* JavaStringToCString(JNIEnv* env, jstring str)
{
    if (!str)
        return nullptr;

    const char* chars = env->GetStringUTFChars(str, nullptr);
    if (!chars)
        return nullptr;

    size_t len = strlen(chars);
    char* result = new char[len + 1];
    strcpy(result, chars);

    env->ReleaseStringUTFChars(str, chars);
    return result;
}

jstring CStringToJavaString(JNIEnv* env, const char* str)
{
    if (!str)
        return nullptr;
    return env->NewStringUTF(str);
}

} // namespace MTA::Android::JNI
