/*
 * MTA:SA Android - Test JNI Interface
 *
 * Native methods for running tests from Java.
 */

#include "TestHarness.h"

#include <jni.h>
#include <string>
#include <vector>
#include <thread>
#include <cstdlib>
#include <cstdio>

#ifdef __ANDROID__
#include <sys/system_properties.h>
#endif

using namespace MTA::Android::Test;

// Forward declare test registration (from SubsystemTests.cpp)
extern void RegisterAllTests();

//=============================================================================
// JNI Helper Functions
//=============================================================================

static jstring ToJString(JNIEnv* env, const std::string& str)
{
    return env->NewStringUTF(str.c_str());
}

static jobjectArray ToStringArray(JNIEnv* env, const std::vector<std::string>& strings)
{
    jclass stringClass = env->FindClass("java/lang/String");
    jobjectArray array = env->NewObjectArray(strings.size(), stringClass, nullptr);

    for (size_t i = 0; i < strings.size(); i++)
    {
        env->SetObjectArrayElement(array, i, ToJString(env, strings[i]));
    }

    return array;
}

//=============================================================================
// Test Result to Java Object
//=============================================================================

static jobject CreateTestResultObject(JNIEnv* env, const TestResult& result)
{
    // Find the TestResult Java class
    jclass resultClass = env->FindClass("com/mtasa/android/test/TestResult");
    if (!resultClass)
    {
        return nullptr;
    }

    // Get constructor
    jmethodID constructor = env->GetMethodID(resultClass, "<init>",
        "(Ljava/lang/String;ILjava/lang/String;F)V");
    if (!constructor)
    {
        return nullptr;
    }

    // Create object
    jstring name = ToJString(env, result.name);
    jint status = static_cast<jint>(result.status);
    jstring message = ToJString(env, result.message);
    jfloat duration = result.durationMs;

    return env->NewObject(resultClass, constructor, name, status, message, duration);
}

static jobjectArray CreateTestResultArray(JNIEnv* env, const std::vector<TestResult>& results)
{
    jclass resultClass = env->FindClass("com/mtasa/android/test/TestResult");
    if (!resultClass)
    {
        return nullptr;
    }

    jobjectArray array = env->NewObjectArray(results.size(), resultClass, nullptr);

    for (size_t i = 0; i < results.size(); i++)
    {
        jobject obj = CreateTestResultObject(env, results[i]);
        if (obj)
        {
            env->SetObjectArrayElement(array, i, obj);
        }
    }

    return array;
}

//=============================================================================
// JNI Native Methods
//=============================================================================

extern "C" {

/**
 * Get the number of registered tests
 */
JNIEXPORT jint JNICALL
Java_com_mtasa_android_test_MTATest_nativeGetTestCount(JNIEnv* env, jclass clazz)
{
    return static_cast<jint>(TestHarness::Instance().GetTestCount());
}

/**
 * Get list of test categories
 */
JNIEXPORT jobjectArray JNICALL
Java_com_mtasa_android_test_MTATest_nativeGetCategories(JNIEnv* env, jclass clazz)
{
    auto categories = TestHarness::Instance().GetCategories();
    return ToStringArray(env, categories);
}

/**
 * Run all tests
 */
JNIEXPORT jobjectArray JNICALL
Java_com_mtasa_android_test_MTATest_nativeRunAllTests(JNIEnv* env, jclass clazz)
{
    auto results = TestHarness::Instance().RunAll();
    return CreateTestResultArray(env, results);
}

/**
 * Run tests in a specific category
 */
JNIEXPORT jobjectArray JNICALL
Java_com_mtasa_android_test_MTATest_nativeRunCategory(JNIEnv* env, jclass clazz, jstring category)
{
    const char* categoryStr = env->GetStringUTFChars(category, nullptr);
    auto results = TestHarness::Instance().RunCategory(categoryStr);
    env->ReleaseStringUTFChars(category, categoryStr);

    return CreateTestResultArray(env, results);
}

/**
 * Get test report as string
 */
JNIEXPORT jstring JNICALL
Java_com_mtasa_android_test_MTATest_nativeGetReport(JNIEnv* env, jclass clazz, jobjectArray results)
{
    // For now, just run all tests and generate report
    auto testResults = TestHarness::Instance().RunAll();
    std::string report = TestHarness::Instance().GenerateReport(testResults);
    return ToJString(env, report);
}

/**
 * Quick self-test - returns true if basic systems work
 */
JNIEXPORT jboolean JNICALL
Java_com_mtasa_android_test_MTATest_nativeQuickTest(JNIEnv* env, jclass clazz)
{
    TEST_LOG("Running quick self-test...");

    // Test 1: Memory allocation
    void* ptr = malloc(1024);
    if (!ptr)
    {
        TEST_ERR("Memory allocation failed");
        return JNI_FALSE;
    }
    free(ptr);

    // Test 2: Basic threading
    bool threadOk = true;
    std::thread t([&threadOk]() {
        threadOk = true;
    });
    t.join();
    if (!threadOk)
    {
        TEST_ERR("Threading failed");
        return JNI_FALSE;
    }

    // Test 3: File access
    FILE* f = fopen("/proc/self/maps", "r");
    if (!f)
    {
        TEST_ERR("Cannot access /proc/self/maps");
        return JNI_FALSE;
    }
    fclose(f);

    TEST_LOG("Quick self-test PASSED");
    return JNI_TRUE;
}

/**
 * Get device info string
 */
JNIEXPORT jstring JNICALL
Java_com_mtasa_android_test_MTATest_nativeGetDeviceInfo(JNIEnv* env, jclass clazz)
{
    std::string info;

#ifdef __ANDROID__
    char sdk[92] = {0};
    char model[92] = {0};
    char abi[92] = {0};

    __system_property_get("ro.build.version.sdk", sdk);
    __system_property_get("ro.product.model", model);
    __system_property_get("ro.product.cpu.abi", abi);

    info = "Model: ";
    info += model;
    info += "\nSDK: ";
    info += sdk;
    info += "\nABI: ";
    info += abi;
    info += "\nArch: ";

#if defined(__aarch64__)
    info += "ARM64";
#elif defined(__arm__)
    info += "ARM32";
#else
    info += "Unknown";
#endif

#else
    info = "Not running on Android";
#endif

    return ToJString(env, info);
}

} // extern "C"
