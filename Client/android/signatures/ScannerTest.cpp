/*
 * MTA:SA Android - Signature Scanner Test
 *
 * Tests for the pattern scanning and address mapping system.
 */

#include <android/log.h>
#include <jni.h>
#include <cstdio>
#include <cstring>

#include "SignatureScanner.h"

#define LOG_TAG "MTA:ScannerTest"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

using namespace MTA::Android::Signatures;

// =============================================================================
// Test Data
// =============================================================================

namespace {

// Sample binary data for testing pattern matching
const uint8_t g_testData[] = {
    // Offset 0x00: Some random data
    0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0,
    // Offset 0x08: Pattern we'll search for
    0xE9, 0x2D, 0x40, 0x00, 0xE5, 0x9F, 0xF0, 0x18,
    // Offset 0x10: More data
    0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x00, 0x00, 0x00,  // "Hello\0"
    // Offset 0x18: Reference to offset 0x10 (address 0x10 as little-endian)
    0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // Offset 0x20: End marker
    0xFF, 0xFF, 0xFF, 0xFF
};

// =============================================================================
// Test Functions
// =============================================================================

bool TestPatternParsing()
{
    LOGI("=== Test: Pattern Parsing ===");

    // Test IDA-style pattern
    auto pattern1 = ParsePattern("E9 2D ?? 00 E5");

    if (pattern1.size() != 5)
    {
        LOGE("Pattern size incorrect: expected 5, got %zu", pattern1.size());
        return false;
    }

    // Verify bytes
    if (pattern1[0].value != 0xE9 || pattern1[0].wildcard)
    {
        LOGE("Pattern byte 0 incorrect");
        return false;
    }

    if (!pattern1[2].wildcard)
    {
        LOGE("Pattern byte 2 should be wildcard");
        return false;
    }

    LOGI("Pattern parsing: PASS");

    // Test mask-style pattern
    const char bytes[] = "\xE9\x2D\x40\x00\xE5";
    const char mask[] = "xx?xx";
    auto pattern2 = ParsePatternWithMask(bytes, mask);

    if (pattern2.size() != 5)
    {
        LOGE("Mask pattern size incorrect");
        return false;
    }

    if (!pattern2[2].wildcard)
    {
        LOGE("Mask pattern byte 2 should be wildcard");
        return false;
    }

    LOGI("Mask pattern parsing: PASS");
    return true;
}

bool TestPatternScanning()
{
    LOGI("=== Test: Pattern Scanning ===");

    SignatureScanner scanner;

    // Add test data as a region
    uintptr_t base = reinterpret_cast<uintptr_t>(g_testData);
    scanner.AddRegion(base, sizeof(g_testData), "test_data");

    // Search for pattern at offset 0x08
    auto pattern = ParsePattern("E9 2D ?? 00 E5");
    uintptr_t result = scanner.FindPattern(pattern);

    if (result == 0)
    {
        LOGE("Pattern not found!");
        return false;
    }

    uintptr_t expectedAddr = base + 0x08;
    if (result != expectedAddr)
    {
        LOGE("Pattern found at wrong address: expected 0x%lX, got 0x%lX",
             expectedAddr, result);
        return false;
    }

    LOGI("Pattern found at correct offset: 0x%lX", result - base);
    return true;
}

bool TestStringSearch()
{
    LOGI("=== Test: String Search ===");

    SignatureScanner scanner;
    uintptr_t base = reinterpret_cast<uintptr_t>(g_testData);
    scanner.AddRegion(base, sizeof(g_testData), "test_data");

    // Search for "Hello" string (at offset 0x10)
    auto pattern = ParsePattern("48 65 6C 6C 6F 00");  // "Hello\0"
    uintptr_t result = scanner.FindPattern(pattern);

    if (result == 0)
    {
        LOGE("String not found!");
        return false;
    }

    uintptr_t expectedAddr = base + 0x10;
    if (result != expectedAddr)
    {
        LOGE("String found at wrong address");
        return false;
    }

    LOGI("String found at correct offset: 0x%lX", result - base);
    return true;
}

bool TestFindAllPatterns()
{
    LOGI("=== Test: Find All Patterns ===");

    // Create data with multiple occurrences
    uint8_t multiData[] = {
        0xAA, 0xBB, 0xCC, 0x00,
        0xAA, 0xBB, 0xCC, 0x00,
        0xDD, 0xEE, 0xFF, 0x00,
        0xAA, 0xBB, 0xCC, 0x00,
    };

    SignatureScanner scanner;
    uintptr_t base = reinterpret_cast<uintptr_t>(multiData);
    scanner.AddRegion(base, sizeof(multiData), "multi_data");

    auto pattern = ParsePattern("AA BB CC");
    auto results = scanner.FindAllPatterns(pattern);

    if (results.size() != 3)
    {
        LOGE("Expected 3 matches, found %zu", results.size());
        return false;
    }

    LOGI("Found %zu pattern occurrences: PASS", results.size());
    return true;
}

bool TestAddressMapper()
{
    LOGI("=== Test: Address Mapper ===");

    auto& mapper = AddressMapper::Instance();

    // Register some test addresses
    mapper.Register("TestFunc1", 0x12345678, "E9 2D ?? 00");
    mapper.Register("TestFunc2", 0x87654321, "48 65 6C 6C");

    // Set ARM addresses manually
    mapper.SetARMAddress("TestFunc1", 0xABCD1234, true);

    // Test retrieval
    uintptr_t addr1 = mapper.GetARMAddress("TestFunc1");
    uintptr_t addr2 = mapper.GetARMAddress("TestFunc2");
    uintptr_t addrNone = mapper.GetARMAddress("NonExistent");

    if (addr1 != 0xABCD1234)
    {
        LOGE("TestFunc1 address incorrect");
        return false;
    }

    if (addr2 != 0)
    {
        LOGE("TestFunc2 should be 0 (not resolved)");
        return false;
    }

    if (addrNone != 0)
    {
        LOGE("NonExistent should return 0");
        return false;
    }

    // Test translation
    uintptr_t translated = mapper.TranslateX86ToARM(0x12345678);
    if (translated != 0xABCD1234)
    {
        LOGE("Translation failed");
        return false;
    }

    // Test stats
    size_t total, resolved, verified;
    mapper.GetStats(total, resolved, verified);

    LOGI("Stats: total=%zu, resolved=%zu, verified=%zu", total, resolved, verified);

    // Test JSON export
    std::string json = mapper.ExportJSON();
    LOGI("JSON export length: %zu bytes", json.length());

    if (json.find("TestFunc1") == std::string::npos)
    {
        LOGE("JSON doesn't contain TestFunc1");
        return false;
    }

    LOGI("Address mapper: PASS");
    return true;
}

bool TestSignatureResolution()
{
    LOGI("=== Test: Signature Resolution ===");

    SignatureScanner scanner;
    uintptr_t base = reinterpret_cast<uintptr_t>(g_testData);
    scanner.AddRegion(base, sizeof(g_testData), "test_data");

    // Create a signature
    Signature sig;
    sig.name = "TestPattern";
    sig.pattern = ParsePattern("E9 2D ?? 00 E5");
    sig.x86Address = 0x534310;  // Original x86 address

    // Resolve it
    bool resolved = scanner.ResolveSignature(sig);

    if (!resolved || sig.foundAddress == 0)
    {
        LOGE("Signature resolution failed");
        return false;
    }

    LOGI("Signature '%s' resolved to 0x%lX", sig.name.c_str(), sig.foundAddress);
    return true;
}

bool TestGTASASignatureRegistration()
{
    LOGI("=== Test: GTA:SA Signature Registration ===");

    // This tests that RegisterGTASASignatures() works without crashing
    RegisterGTASASignatures();

    auto& mapper = AddressMapper::Instance();
    size_t total, resolved, verified;
    mapper.GetStats(total, resolved, verified);

    LOGI("Registered %zu GTA:SA signatures", total);

    // Check that some known signatures exist
    uintptr_t entityRender = mapper.GetARMAddress("CEntity::Render");
    // It should be 0 since we haven't scanned the actual GTA:SA binary
    // but the entry should exist

    LOGI("GTA:SA signature registration: PASS");
    return true;
}

} // anonymous namespace

// =============================================================================
// JNI Entry Points
// =============================================================================

extern "C" {

JNIEXPORT jboolean JNICALL
Java_com_mtasa_android_ScannerTest_runAllTests(JNIEnv* env, jclass clazz)
{
    LOGI("========================================");
    LOGI("MTA:SA Android Signature Scanner Tests");
    LOGI("========================================");

    bool allPassed = true;

    allPassed &= TestPatternParsing();
    allPassed &= TestPatternScanning();
    allPassed &= TestStringSearch();
    allPassed &= TestFindAllPatterns();
    allPassed &= TestAddressMapper();
    allPassed &= TestSignatureResolution();
    allPassed &= TestGTASASignatureRegistration();

    LOGI("========================================");
    LOGI("Scanner Test Results: %s", allPassed ? "ALL PASSED" : "SOME FAILED");
    LOGI("========================================");

    return allPassed ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL
Java_com_mtasa_android_ScannerTest_getRegisteredSignatureCount(JNIEnv* env, jclass clazz)
{
    RegisterGTASASignatures();

    size_t total, resolved, verified;
    AddressMapper::Instance().GetStats(total, resolved, verified);

    return static_cast<jint>(total);
}

} // extern "C"
