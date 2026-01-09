/*
 * MTA:SA Android - JNI Bridge Header
 *
 * Defines the native interface between Java and C++ code.
 * This is the main entry point for the Android app to communicate with MTA.
 *
 * Java class: com.mtasa.android.MTANative
 */

#ifndef MTA_NATIVE_H
#define MTA_NATIVE_H

#include <jni.h>
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// JNI Library Entry Points
//=============================================================================

/**
 * Called when the native library is loaded
 */
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved);

/**
 * Called when the native library is unloaded
 */
JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved);

//=============================================================================
// Core MTA Functions (com.mtasa.android.MTANative)
//=============================================================================

/**
 * Initialize MTA client
 * @param env JNI environment
 * @param obj MTANative instance
 * @param context Android application context
 * @param assetManager Android asset manager
 * @return true if initialization succeeded
 */
JNIEXPORT jboolean JNICALL Java_com_mtasa_android_MTANative_nativeInit(
    JNIEnv* env, jobject obj, jobject context, jobject assetManager);

/**
 * Shutdown MTA client
 */
JNIEXPORT void JNICALL Java_com_mtasa_android_MTANative_nativeShutdown(
    JNIEnv* env, jobject obj);

/**
 * Main game loop tick
 * @param deltaTime Time since last frame (milliseconds)
 */
JNIEXPORT void JNICALL Java_com_mtasa_android_MTANative_nativeUpdate(
    JNIEnv* env, jobject obj, jfloat deltaTime);

/**
 * Render frame
 */
JNIEXPORT void JNICALL Java_com_mtasa_android_MTANative_nativeRender(
    JNIEnv* env, jobject obj);

/**
 * Called when surface is created/changed
 * @param width Surface width in pixels
 * @param height Surface height in pixels
 */
JNIEXPORT void JNICALL Java_com_mtasa_android_MTANative_nativeSurfaceChanged(
    JNIEnv* env, jobject obj, jint width, jint height);

/**
 * Called when surface is destroyed
 */
JNIEXPORT void JNICALL Java_com_mtasa_android_MTANative_nativeSurfaceDestroyed(
    JNIEnv* env, jobject obj);

/**
 * Called when app is paused
 */
JNIEXPORT void JNICALL Java_com_mtasa_android_MTANative_nativePause(
    JNIEnv* env, jobject obj);

/**
 * Called when app is resumed
 */
JNIEXPORT void JNICALL Java_com_mtasa_android_MTANative_nativeResume(
    JNIEnv* env, jobject obj);

//=============================================================================
// Input Events
//=============================================================================

/**
 * Touch event
 * @param action MotionEvent action (ACTION_DOWN, ACTION_UP, ACTION_MOVE, etc.)
 * @param pointerId Pointer/finger ID
 * @param x X coordinate
 * @param y Y coordinate
 * @param pressure Touch pressure (0-1)
 */
JNIEXPORT void JNICALL Java_com_mtasa_android_MTANative_nativeTouchEvent(
    JNIEnv* env, jobject obj, jint action, jint pointerId,
    jfloat x, jfloat y, jfloat pressure);

/**
 * Key event
 * @param keyCode Android key code
 * @param action KeyEvent action (ACTION_DOWN, ACTION_UP)
 * @param metaState Meta key state
 */
JNIEXPORT void JNICALL Java_com_mtasa_android_MTANative_nativeKeyEvent(
    JNIEnv* env, jobject obj, jint keyCode, jint action, jint metaState);

/**
 * Gamepad connected
 * @param deviceId Device ID
 * @param name Device name
 */
JNIEXPORT void JNICALL Java_com_mtasa_android_MTANative_nativeGamepadConnected(
    JNIEnv* env, jobject obj, jint deviceId, jstring name);

/**
 * Gamepad disconnected
 * @param deviceId Device ID
 */
JNIEXPORT void JNICALL Java_com_mtasa_android_MTANative_nativeGamepadDisconnected(
    JNIEnv* env, jobject obj, jint deviceId);

/**
 * Gamepad button event
 * @param deviceId Device ID
 * @param button Button code
 * @param pressed True if pressed, false if released
 */
JNIEXPORT void JNICALL Java_com_mtasa_android_MTANative_nativeGamepadButton(
    JNIEnv* env, jobject obj, jint deviceId, jint button, jboolean pressed);

/**
 * Gamepad axis event
 * @param deviceId Device ID
 * @param axis Axis code
 * @param value Axis value (-1 to 1)
 */
JNIEXPORT void JNICALL Java_com_mtasa_android_MTANative_nativeGamepadAxis(
    JNIEnv* env, jobject obj, jint deviceId, jint axis, jfloat value);

//=============================================================================
// Network Events
//=============================================================================

/**
 * Network state changed
 * @param available True if network is available
 * @param type Network type (0=mobile, 1=wifi, etc.)
 * @param metered True if connection is metered
 */
JNIEXPORT void JNICALL Java_com_mtasa_android_MTANative_nativeNetworkStateChanged(
    JNIEnv* env, jobject obj, jboolean available, jint type, jboolean metered);

//=============================================================================
// Server Connection
//=============================================================================

/**
 * Connect to a server
 * @param host Server hostname or IP
 * @param port Server port
 * @param nick Player nickname
 * @param password Server password (empty if none)
 * @return true if connection initiated
 */
JNIEXPORT jboolean JNICALL Java_com_mtasa_android_MTANative_nativeConnect(
    JNIEnv* env, jobject obj, jstring host, jint port,
    jstring nick, jstring password);

/**
 * Disconnect from current server
 */
JNIEXPORT void JNICALL Java_com_mtasa_android_MTANative_nativeDisconnect(
    JNIEnv* env, jobject obj);

/**
 * Get connection state
 * @return Connection state (0=disconnected, 1=connecting, 2=connected)
 */
JNIEXPORT jint JNICALL Java_com_mtasa_android_MTANative_nativeGetConnectionState(
    JNIEnv* env, jobject obj);

//=============================================================================
// Configuration
//=============================================================================

/**
 * Set game data path (GTA:SA files location)
 * @param path Path to game data
 */
JNIEXPORT void JNICALL Java_com_mtasa_android_MTANative_nativeSetGameDataPath(
    JNIEnv* env, jobject obj, jstring path);

/**
 * Get version string
 * @return MTA version string
 */
JNIEXPORT jstring JNICALL Java_com_mtasa_android_MTANative_nativeGetVersion(
    JNIEnv* env, jobject obj);

/**
 * Get build date
 * @return Build date string
 */
JNIEXPORT jstring JNICALL Java_com_mtasa_android_MTANative_nativeGetBuildDate(
    JNIEnv* env, jobject obj);

//=============================================================================
// Callbacks from Native to Java
//=============================================================================

namespace MTA::Android::JNI
{

/**
 * JNI Helper class for calling back to Java
 */
class JavaBridge
{
public:
    static JavaBridge& Instance();

    // Initialize with JNI environment
    bool Initialize(JNIEnv* env, jobject activity);
    void Shutdown();

    // Get JNI environment for current thread
    JNIEnv* GetEnv();

    // Call Java methods
    void ShowToast(const char* message);
    void ShowDialog(const char* title, const char* message);
    void RequestPermission(const char* permission);
    void OpenURL(const char* url);
    void SetClipboardText(const char* text);
    const char* GetClipboardText();

    // UI updates
    void UpdateLoadingProgress(int percent, const char* status);
    void ShowServerBrowser();
    void HideServerBrowser();

    // Keyboard
    void ShowKeyboard(bool show);
    bool IsKeyboardVisible();

    // Vibration
    void Vibrate(int milliseconds);

private:
    JavaBridge();
    ~JavaBridge();

    JavaVM* m_javaVM;
    jobject m_activity;
    jclass m_bridgeClass;

    // Method IDs
    jmethodID m_showToast;
    jmethodID m_showDialog;
    jmethodID m_requestPermission;
    jmethodID m_openURL;
    jmethodID m_setClipboard;
    jmethodID m_getClipboard;
    jmethodID m_updateProgress;
    jmethodID m_showKeyboard;
    jmethodID m_vibrate;
};

/**
 * Get current JNI environment
 * Handles attachment for non-Java threads
 */
JNIEnv* GetJNIEnv();

/**
 * Convert Java string to C string
 * Caller must delete[] the returned string
 */
char* JavaStringToCString(JNIEnv* env, jstring str);

/**
 * Convert C string to Java string
 */
jstring CStringToJavaString(JNIEnv* env, const char* str);

} // namespace MTA::Android::JNI

#ifdef __cplusplus
}
#endif

#endif // MTA_NATIVE_H
