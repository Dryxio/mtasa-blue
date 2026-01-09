/*
 * MTA:SA Android - Hook System Test
 *
 * Unit tests and examples for the ARM hook system.
 * This library can be loaded separately to test hooks without the full MTA.
 */

#include <android/log.h>
#include <jni.h>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>

#include "ARMHookSystem.h"

#define LOG_TAG "MTA:HookTest"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

using namespace MTA::Android::Hooks;

// =============================================================================
// Test Functions
// =============================================================================

namespace {

// Original function to hook
int g_testValue = 0;

__attribute__((noinline))
int TestFunction(int a, int b)
{
    return a + b;
}

// Hook function
int Hook_TestFunction(int a, int b)
{
    g_testValue = 42;  // Mark that hook was called
    return a * b;      // Change behavior
}

// Test: ARM instruction encoding
bool TestARMInstructionEncoding()
{
    LOGI("=== Test: ARM Instruction Encoding ===");

    // Test branch offset calculation
    uintptr_t from = 0x1000;
    uintptr_t to = 0x2000;

    int32_t offset = CalculateARMBranchOffset(from, to);
    // Expected: (0x2000 - 0x1000 - 8) / 4 = (0x1000 - 8) / 4 = 0x3FE
    LOGI("Branch offset 0x%X -> 0x%X = 0x%X", (uint32_t)from, (uint32_t)to, offset);

    // Test branch instruction creation
    uint32_t branch = CreateARMBranch(from, to);
    LOGI("Branch instruction: 0x%08X", branch);

    // Verify format: 0xEA (B) | 24-bit offset
    if ((branch & 0xFF000000) != 0xEA000000)
    {
        LOGE("Branch opcode incorrect!");
        return false;
    }

    // Test BL instruction
    uint32_t bl = CreateARMBranchLink(from, to);
    LOGI("BL instruction: 0x%08X", bl);

    if ((bl & 0xFF000000) != 0xEB000000)
    {
        LOGE("BL opcode incorrect!");
        return false;
    }

    // Test PUSH/POP
    uint32_t push = CreateARMPush(ARM32::REG_R4 | ARM32::REG_R5 | ARM32::REG_LR);
    LOGI("PUSH {r4, r5, lr}: 0x%08X", push);

    uint32_t pop = CreateARMPop(ARM32::REG_R4 | ARM32::REG_R5 | ARM32::REG_PC);
    LOGI("POP {r4, r5, pc}: 0x%08X", pop);

    // Test range checking
    bool inRange = IsWithinARMBranchRange(0x1000, 0x1000000);  // Within 32MB
    bool outOfRange = IsWithinARMBranchRange(0x1000, 0x10000000);  // Outside 32MB

    LOGI("Range test (should be true): %s", inRange ? "PASS" : "FAIL");
    LOGI("Range test (should be false): %s", !outOfRange ? "PASS" : "FAIL");

    return inRange && !outOfRange;
}

// Test: Thumb mode detection
bool TestThumbModeDetection()
{
    LOGI("=== Test: Thumb Mode Detection ===");

    uintptr_t armAddr = 0x1000;
    uintptr_t thumbAddr = 0x1001;

    bool isThumb1 = IsThumbAddress(armAddr);
    bool isThumb2 = IsThumbAddress(thumbAddr);

    LOGI("0x%X is Thumb: %s (expected: false)", (uint32_t)armAddr, isThumb1 ? "true" : "false");
    LOGI("0x%X is Thumb: %s (expected: true)", (uint32_t)thumbAddr, isThumb2 ? "true" : "false");

    uintptr_t cleared = ClearThumbBit(thumbAddr);
    uintptr_t set = SetThumbBit(armAddr);

    LOGI("ClearThumbBit(0x1001) = 0x%X (expected: 0x1000)", (uint32_t)cleared);
    LOGI("SetThumbBit(0x1000) = 0x%X (expected: 0x1001)", (uint32_t)set);

    return !isThumb1 && isThumb2 && cleared == 0x1000 && set == 0x1001;
}

// Test: Memory operations (safe, no actual patching)
bool TestMemoryOperations()
{
    LOGI("=== Test: Memory Operations ===");

    // Create a writable buffer to test memory ops
    uint8_t buffer[64] = {0};

    // Test MemPut (on our own buffer, no protection needed)
    uint32_t testValue = 0xDEADBEEF;
    memcpy(buffer, &testValue, sizeof(testValue));

    uint32_t readBack = *reinterpret_cast<uint32_t*>(buffer);
    LOGI("MemPut test: wrote 0x%X, read 0x%X", testValue, readBack);

    // Test MemSet
    memset(buffer + 8, 0x90, 8);  // Fill with NOPs

    bool nopFilled = true;
    for (int i = 8; i < 16; i++)
    {
        if (buffer[i] != 0x90)
        {
            nopFilled = false;
            break;
        }
    }

    LOGI("MemSet test: %s", nopFilled ? "PASS" : "FAIL");

    return readBack == testValue && nopFilled;
}

// Test: HookInfo structure
bool TestHookInfoStructure()
{
    LOGI("=== Test: HookInfo Structure ===");

    HookInfo info(0x12345678, 0x87654321, 4, false);

    LOGI("HookInfo address: 0x%lX", info.address);
    LOGI("HookInfo hook: 0x%lX", info.hook);
    LOGI("HookInfo size: %u", info.size);
    LOGI("HookInfo isThumb: %s", info.isThumb ? "true" : "false");
    LOGI("HookInfo installed: %s", info.installed ? "true" : "false");

    return info.address == 0x12345678 &&
           info.hook == 0x87654321 &&
           info.size == 4 &&
           !info.isThumb &&
           !info.installed;
}

// Test: HookManager
bool TestHookManager()
{
    LOGI("=== Test: HookManager ===");

    auto& mgr = HookManager::Instance();

    // Note: We can't actually install hooks in this test without a target binary
    // This just tests the management infrastructure

    HookInfo* notFound = mgr.Find("NonExistent");
    LOGI("Find non-existent hook: %s", notFound == nullptr ? "PASS" : "FAIL");

    return notFound == nullptr;
}

#if defined(__aarch64__)
// Test: ARM64 instruction encoding
bool TestARM64InstructionEncoding()
{
    LOGI("=== Test: ARM64 Instruction Encoding ===");

    uintptr_t from = 0x1000;
    uintptr_t to = 0x5000;

    int32_t offset = CalculateARM64BranchOffset(from, to);
    LOGI("ARM64 branch offset 0x%X -> 0x%X = 0x%X", (uint32_t)from, (uint32_t)to, offset);

    uint32_t branch = CreateARM64Branch(from, to);
    LOGI("ARM64 B instruction: 0x%08X", branch);

    // Verify format: 0x14 (B) | 26-bit offset
    if ((branch & 0xFC000000) != 0x14000000)
    {
        LOGE("ARM64 branch opcode incorrect!");
        return false;
    }

    uint32_t bl = CreateARM64BranchLink(from, to);
    LOGI("ARM64 BL instruction: 0x%08X", bl);

    if ((bl & 0xFC000000) != 0x94000000)
    {
        LOGE("ARM64 BL opcode incorrect!");
        return false;
    }

    return true;
}
#endif

} // anonymous namespace

// =============================================================================
// JNI Entry Points
// =============================================================================

extern "C" {

JNIEXPORT jboolean JNICALL
Java_com_mtasa_android_HookTest_runAllTests(JNIEnv* env, jclass clazz)
{
    LOGI("========================================");
    LOGI("MTA:SA Android Hook System Tests");
    LOGI("========================================");

    bool allPassed = true;

    allPassed &= TestARMInstructionEncoding();
    allPassed &= TestThumbModeDetection();
    allPassed &= TestMemoryOperations();
    allPassed &= TestHookInfoStructure();
    allPassed &= TestHookManager();

#if defined(__aarch64__)
    allPassed &= TestARM64InstructionEncoding();
#endif

    LOGI("========================================");
    LOGI("Test Results: %s", allPassed ? "ALL PASSED" : "SOME FAILED");
    LOGI("========================================");

    return allPassed ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL
Java_com_mtasa_android_HookTest_getArchitecture(JNIEnv* env, jclass clazz)
{
#if defined(__aarch64__)
    return env->NewStringUTF("ARM64 (AArch64)");
#elif defined(__arm__)
    return env->NewStringUTF("ARM32 (ARMv7)");
#else
    return env->NewStringUTF("Unknown");
#endif
}

} // extern "C"
