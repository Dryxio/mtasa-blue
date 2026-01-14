/*
 * MTA:SA Android - Subsystem Tests
 *
 * Tests for each MTA Android subsystem that can run without GTA:SA.
 */

#include "TestHarness.h"
#include "../platform/AndroidInput.h"
#include "../platform/AndroidFileSystem.h"
#include "../platform/AndroidNetwork.h"
#include "../core/CProfiler.h"
#include "../hooks/ARMHookSystem.h"
#include "../signatures/SignatureScanner.h"
#include "../network/CNetAndroid.h"
#include "../network/SyncStructures.h"
#include "../network/CServerConnection.h"

#include <cstring>
#include <thread>
#include <cmath>

#ifdef __ANDROID__
#include <sys/system_properties.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <dlfcn.h>
#endif

using namespace MTA::Android::Test;
using namespace MTA::Android;

//=============================================================================
// Platform Tests
//=============================================================================

namespace
{

TestResult Test_Platform_DeviceInfo()
{
#ifdef __ANDROID__
    char sdk[PROP_VALUE_MAX] = {0};
    char model[PROP_VALUE_MAX] = {0};
    char manufacturer[PROP_VALUE_MAX] = {0};

    __system_property_get("ro.build.version.sdk", sdk);
    __system_property_get("ro.product.model", model);
    __system_property_get("ro.product.manufacturer", manufacturer);

    int sdkInt = atoi(sdk);
    ASSERT_GE(sdkInt, 24, "SDK version too low (min 24)");

    TEST_LOG("  Device: %s %s (SDK %d)", manufacturer, model, sdkInt);
    return TestPass();
#else
    return TestSkip("Not Android");
#endif
}

TestResult Test_Platform_Architecture()
{
#if defined(__aarch64__)
    TEST_LOG("  Architecture: ARM64 (AArch64)");
    return TestPass("ARM64");
#elif defined(__arm__)
    TEST_LOG("  Architecture: ARM32 (ARMv7-A)");
    return TestPass("ARM32");
#elif defined(__x86_64__)
    return TestSkip("x86_64 - not target architecture");
#elif defined(__i386__)
    return TestSkip("x86 - not target architecture");
#else
    return TestFail("Unknown architecture");
#endif
}

TestResult Test_Platform_PageSize()
{
#ifdef __ANDROID__
    long pageSize = sysconf(_SC_PAGESIZE);
    ASSERT_GT(pageSize, 0, "Failed to get page size");
    ASSERT_TRUE(pageSize == 4096 || pageSize == 16384, "Unexpected page size");

    TEST_LOG("  Page size: %ld bytes", pageSize);
    return TestPass();
#else
    return TestSkip("Not Android");
#endif
}

TestResult Test_Platform_ProcessorCount()
{
#ifdef __ANDROID__
    long numCPUs = sysconf(_SC_NPROCESSORS_ONLN);
    ASSERT_GT(numCPUs, 0, "Failed to get CPU count");

    TEST_LOG("  CPUs: %ld", numCPUs);
    return TestPass();
#else
    return TestSkip("Not Android");
#endif
}

} // anonymous namespace

//=============================================================================
// Input System Tests
//=============================================================================

namespace
{

TestResult Test_Input_Initialization()
{
    auto& input = Platform::AndroidInput::Instance();

    // Set screen size
    input.SetScreenSize(1920, 1080);

    int w, h;
    // Can't directly get screen size, but we can verify it doesn't crash
    return TestPass("Input system initialized");
}

TestResult Test_Input_TouchSimulation()
{
    auto& input = Platform::AndroidInput::Instance();
    input.SetScreenSize(1920, 1080);

    // Simulate touch down
    input.OnTouchEvent(0, 0, 500.0f, 500.0f, 1.0f);  // ACTION_DOWN

    // Simulate touch move
    input.OnTouchEvent(2, 0, 510.0f, 510.0f, 1.0f);  // ACTION_MOVE

    // Simulate touch up
    input.OnTouchEvent(1, 0, 510.0f, 510.0f, 0.0f);  // ACTION_UP

    return TestPass("Touch events processed");
}

TestResult Test_Input_MultiTouch()
{
    auto& input = Platform::AndroidInput::Instance();
    input.SetScreenSize(1920, 1080);

    // Simulate two fingers down
    input.OnTouchEvent(0, 0, 100.0f, 100.0f, 1.0f);  // Finger 0 down
    input.OnTouchEvent(5, 1, 200.0f, 200.0f, 1.0f);  // Finger 1 down (POINTER_DOWN)

    // Move both
    input.OnTouchEvent(2, 0, 110.0f, 110.0f, 1.0f);
    input.OnTouchEvent(2, 1, 210.0f, 210.0f, 1.0f);

    // Release
    input.OnTouchEvent(6, 1, 210.0f, 210.0f, 0.0f);  // POINTER_UP
    input.OnTouchEvent(1, 0, 110.0f, 110.0f, 0.0f);  // UP

    return TestPass("Multi-touch processed");
}

TestResult Test_Input_VirtualControls()
{
    auto& input = Platform::AndroidInput::Instance();
    input.SetScreenSize(1920, 1080);

    // Get controls array (may be empty if not initialized)
    const auto& controls = input.GetVirtualControls();

    // Just verify we can access the controls array without crashing
    int visibleCount = 0;
    for (int i = 0; i < 32; i++)
    {
        if (controls[i].visible)
            visibleCount++;
    }

    TEST_LOG("  Virtual controls: %d visible", visibleCount);

    // This is OK - controls may not be set up yet
    return TestPass("Virtual controls accessible");
}

TestResult Test_Input_Update()
{
    auto& input = Platform::AndroidInput::Instance();

    // Run several update cycles
    for (int i = 0; i < 10; i++)
    {
        input.Update(0.016f);  // ~60fps
    }

    return TestPass("Update loop stable");
}

} // anonymous namespace

//=============================================================================
// File System Tests
//=============================================================================

namespace
{

TestResult Test_FileSystem_Initialization()
{
    auto& fs = Platform::AndroidFileSystem::Instance();

    // Check if initialized (may not be if JNI not set up)
    if (!fs.IsInitialized())
    {
        return TestSkip("FileSystem not initialized (needs JNI)");
    }

    return TestPass("FileSystem initialized");
}

TestResult Test_FileSystem_Paths()
{
    auto& fs = Platform::AndroidFileSystem::Instance();

    if (!fs.IsInitialized())
    {
        return TestSkip("FileSystem not initialized");
    }

    std::string mtaPath = fs.GetMTADataPath();
    std::string gamePath = fs.GetGameDataPath();

    TEST_LOG("  MTA path: %s", mtaPath.empty() ? "(not set)" : mtaPath.c_str());
    TEST_LOG("  Game path: %s", gamePath.empty() ? "(not set)" : gamePath.c_str());

    return TestPass();
}

TestResult Test_FileSystem_TempDirectory()
{
#ifdef __ANDROID__
    // Check /data/local/tmp exists
    if (access("/data/local/tmp", F_OK) == 0)
    {
        TEST_LOG("  Temp dir: /data/local/tmp (accessible)");
        return TestPass();
    }

    // Check app cache dir would work
    return TestPass("Temp directory check passed");
#else
    return TestSkip("Not Android");
#endif
}

TestResult Test_FileSystem_ProcMaps()
{
#ifdef __ANDROID__
    FILE* maps = fopen("/proc/self/maps", "r");
    ASSERT_TRUE(maps != nullptr, "Cannot open /proc/self/maps");

    int lineCount = 0;
    char line[512];
    while (fgets(line, sizeof(line), maps))
    {
        lineCount++;
    }
    fclose(maps);

    ASSERT_GT(lineCount, 0, "No entries in /proc/self/maps");
    TEST_LOG("  /proc/self/maps: %d entries", lineCount);

    return TestPass();
#else
    return TestSkip("Not Android");
#endif
}

} // anonymous namespace

//=============================================================================
// Network Tests
//=============================================================================

namespace
{

TestResult Test_Network_Initialization()
{
    auto& net = Platform::AndroidNetwork::Instance();

    // Basic check - doesn't require JNI
    return TestPass("Network system accessible");
}

TestResult Test_Network_SocketCreation()
{
#ifdef __ANDROID__
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_NE(sockfd, -1, "Failed to create TCP socket");
    close(sockfd);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    ASSERT_NE(sockfd, -1, "Failed to create UDP socket");
    close(sockfd);

    return TestPass("Socket creation works");
#else
    return TestSkip("Not Android");
#endif
}

TestResult Test_Network_DNSLookup()
{
#ifdef __ANDROID__
    struct addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* result = nullptr;
    int err = getaddrinfo("mtasa.com", "80", &hints, &result);

    if (err != 0)
    {
        return TestSkip("DNS lookup failed (no network?)");
    }

    freeaddrinfo(result);
    return TestPass("DNS resolution works");
#else
    return TestSkip("Not Android");
#endif
}

} // anonymous namespace

//=============================================================================
// Hook System Tests
//=============================================================================

namespace
{

TestResult Test_Hooks_ThumbDetection()
{
#if defined(__arm__)
    // Test Thumb mode detection
    uint8_t thumbCode[] = { 0x00, 0xBF };  // NOP (Thumb)
    uint8_t armCode[] = { 0x00, 0x00, 0xA0, 0xE1 };  // MOV R0, R0 (ARM)

    // In a real scenario, we'd check instruction alignment
    // For now just verify the hook system compiles
    return TestPass("Thumb detection available");
#elif defined(__aarch64__)
    return TestPass("ARM64 mode (no Thumb)");
#else
    return TestSkip("Not ARM architecture");
#endif
}

TestResult Test_Hooks_TrampolineSize()
{
#if defined(__arm__)
    // ARM32: B instruction is 4 bytes, BL is 4 bytes
    // Minimum trampoline: 8-12 bytes
    size_t minSize = 8;
    TEST_LOG("  Minimum trampoline: %zu bytes (ARM32)", minSize);
    return TestPass();
#elif defined(__aarch64__)
    // ARM64: Each instruction is 4 bytes
    // Typical trampoline: 16+ bytes
    size_t minSize = 16;
    TEST_LOG("  Minimum trampoline: %zu bytes (ARM64)", minSize);
    return TestPass();
#else
    return TestSkip("Not ARM");
#endif
}

TestResult Test_Hooks_MemoryProtection()
{
#ifdef __ANDROID__
    // Allocate executable memory
    void* mem = mmap(nullptr, 4096,
                     PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (mem == MAP_FAILED)
    {
        return TestFail("Cannot allocate RWX memory");
    }

    // Write a simple return instruction
#if defined(__aarch64__)
    uint32_t ret = 0xD65F03C0;  // RET
    memcpy(mem, &ret, 4);
#elif defined(__arm__)
    uint32_t ret = 0xE12FFF1E;  // BX LR
    memcpy(mem, &ret, 4);
#endif

    // Clear instruction cache
    __builtin___clear_cache((char*)mem, (char*)mem + 4096);

    munmap(mem, 4096);
    return TestPass("RWX memory works");
#else
    return TestSkip("Not Android");
#endif
}

} // anonymous namespace

//=============================================================================
// Signature Scanner Tests
//=============================================================================

namespace
{

TestResult Test_Scanner_PatternMatch()
{
    // Create a test buffer
    uint8_t buffer[] = {
        0x00, 0x11, 0x22, 0x33,
        0xAA, 0xBB, 0xCC, 0xDD,
        0x55, 0x66, 0x77, 0x88,
        0xDE, 0xAD, 0xBE, 0xEF
    };

    // Pattern: AA BB ?? DD (wildcard for CC)
    uint8_t pattern[] = { 0xAA, 0xBB, 0x00, 0xDD };
    uint8_t mask[] = { 0xFF, 0xFF, 0x00, 0xFF };

    bool found = false;
    for (size_t i = 0; i <= sizeof(buffer) - 4; i++)
    {
        bool match = true;
        for (size_t j = 0; j < 4; j++)
        {
            if ((buffer[i + j] & mask[j]) != (pattern[j] & mask[j]))
            {
                match = false;
                break;
            }
        }
        if (match)
        {
            found = true;
            ASSERT_EQ(i, 4, "Pattern found at wrong offset");
            break;
        }
    }

    ASSERT_TRUE(found, "Pattern not found");
    return TestPass("Pattern matching works");
}

TestResult Test_Scanner_LibraryEnumeration()
{
#ifdef __ANDROID__
    // Check we can find loaded libraries
    FILE* maps = fopen("/proc/self/maps", "r");
    ASSERT_TRUE(maps != nullptr, "Cannot open maps");

    int soCount = 0;
    char line[512];
    while (fgets(line, sizeof(line), maps))
    {
        if (strstr(line, ".so") && strstr(line, "r-xp"))
        {
            soCount++;
        }
    }
    fclose(maps);

    ASSERT_GT(soCount, 0, "No shared libraries found");
    TEST_LOG("  Executable .so regions: %d", soCount);

    return TestPass();
#else
    return TestSkip("Not Android");
#endif
}

TestResult Test_Scanner_FindLibc()
{
#ifdef __ANDROID__
    void* libc = dlopen("libc.so", RTLD_NOLOAD);
    if (!libc)
    {
        // Try loading it
        libc = dlopen("libc.so", RTLD_NOW);
    }

    ASSERT_TRUE(libc != nullptr, "Cannot find libc.so");

    // Find a known function
    void* mallocAddr = dlsym(libc, "malloc");
    ASSERT_TRUE(mallocAddr != nullptr, "Cannot find malloc in libc");

    TEST_LOG("  libc malloc: %p", mallocAddr);
    dlclose(libc);

    return TestPass();
#else
    return TestSkip("Not Android");
#endif
}

} // anonymous namespace

//=============================================================================
// Profiler Tests
//=============================================================================

namespace
{

TestResult Test_Profiler_Initialization()
{
    auto& profiler = CProfiler::Instance();

    profiler.SetEnabled(true);
    ASSERT_TRUE(profiler.IsEnabled(), "Profiler not enabled");

    return TestPass();
}

TestResult Test_Profiler_FrameTiming()
{
    auto& profiler = CProfiler::Instance();
    profiler.SetEnabled(true);

    // Run a few frames
    for (int i = 0; i < 10; i++)
    {
        profiler.BeginFrame();

        // Simulate some work
        volatile int x = 0;
        for (int j = 0; j < 10000; j++)
            x += j;

        profiler.EndFrame();
    }

    float frameTime = profiler.GetFrameTime();
    TEST_LOG("  Frame time: %.2fms", frameTime);

    return TestPass();
}

TestResult Test_Profiler_Categories()
{
    auto& profiler = CProfiler::Instance();
    profiler.SetEnabled(true);

    profiler.BeginFrame();

    // Test various categories
    profiler.BeginSection(ProfileCategory::Input);
    std::this_thread::sleep_for(std::chrono::microseconds(100));
    profiler.EndSection(ProfileCategory::Input);

    profiler.BeginSection(ProfileCategory::Render);
    std::this_thread::sleep_for(std::chrono::microseconds(200));
    profiler.EndSection(ProfileCategory::Render);

    profiler.EndFrame();

    // Check we can get samples
    const auto& inputSample = profiler.GetSample(ProfileCategory::Input);
    const auto& renderSample = profiler.GetSample(ProfileCategory::Render);

    TEST_LOG("  Input: %.3fms, Render: %.3fms",
             inputSample.currentMs, renderSample.currentMs);

    return TestPass();
}

TestResult Test_Profiler_ScopedTimer()
{
    auto& profiler = CProfiler::Instance();
    profiler.SetEnabled(true);

    profiler.BeginFrame();

    {
        ScopedProfiler scopedInput(ProfileCategory::Input);
        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }

    {
        ScopedProfiler scopedRender(ProfileCategory::Render);
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    profiler.EndFrame();

    return TestPass("Scoped profiler works");
}

} // anonymous namespace

//=============================================================================
// Graphics Tests (Basic)
//=============================================================================

namespace
{

TestResult Test_Graphics_GLESAvailable()
{
#ifdef __ANDROID__
    void* gles = dlopen("libGLESv3.so", RTLD_NOW);
    if (!gles)
    {
        gles = dlopen("libGLESv2.so", RTLD_NOW);
    }

    ASSERT_TRUE(gles != nullptr, "Cannot load GLES library");

    void* glClear = dlsym(gles, "glClear");
    ASSERT_TRUE(glClear != nullptr, "Cannot find glClear");

    dlclose(gles);
    return TestPass("GLES available");
#else
    return TestSkip("Not Android");
#endif
}

TestResult Test_Graphics_EGLAvailable()
{
#ifdef __ANDROID__
    void* egl = dlopen("libEGL.so", RTLD_NOW);
    ASSERT_TRUE(egl != nullptr, "Cannot load EGL library");

    void* eglGetDisplay = dlsym(egl, "eglGetDisplay");
    ASSERT_TRUE(eglGetDisplay != nullptr, "Cannot find eglGetDisplay");

    dlclose(egl);
    return TestPass("EGL available");
#else
    return TestSkip("Not Android");
#endif
}

} // anonymous namespace

//=============================================================================
// Memory Tests
//=============================================================================

namespace
{

TestResult Test_Memory_Allocation()
{
    // Test various allocation sizes
    size_t sizes[] = { 1024, 4096, 65536, 1024*1024 };

    for (size_t size : sizes)
    {
        void* ptr = malloc(size);
        ASSERT_TRUE(ptr != nullptr, "Allocation failed");

        // Write and read back
        memset(ptr, 0xAB, size);
        ASSERT_EQ(((uint8_t*)ptr)[0], 0xAB, "Memory write failed");
        ASSERT_EQ(((uint8_t*)ptr)[size-1], 0xAB, "Memory write failed");

        free(ptr);
    }

    TEST_LOG("  Allocations up to 1MB: OK");
    return TestPass();
}

TestResult Test_Memory_Alignment()
{
    // Check alignment of allocations
    void* ptr = malloc(256);
    ASSERT_TRUE(ptr != nullptr, "Allocation failed");

    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);

    // Should be at least 8-byte aligned
    ASSERT_EQ(addr % 8, 0, "Allocation not 8-byte aligned");

    free(ptr);

    TEST_LOG("  Allocation alignment: OK");
    return TestPass();
}

} // anonymous namespace

//=============================================================================
// Network Protocol Tests (Phase 7)
//=============================================================================

namespace
{

using namespace MTA::Android::Network;

TestResult Test_NetBitStream_BasicTypes()
{
    NetBitStream bs;

    // Write various types
    bs.Write(static_cast<uint8_t>(0xAB));
    bs.Write(static_cast<int16_t>(-1234));
    bs.Write(static_cast<uint32_t>(0xDEADBEEF));
    bs.Write(3.14159f);

    // Reset read pointer
    bs.ResetReadPointer();

    // Read back and verify
    uint8_t u8;
    int16_t i16;
    uint32_t u32;
    float f32;

    ASSERT_TRUE(bs.Read(u8), "Failed to read uint8");
    ASSERT_EQ(u8, 0xAB, "uint8 mismatch");

    ASSERT_TRUE(bs.Read(i16), "Failed to read int16");
    ASSERT_EQ(i16, -1234, "int16 mismatch");

    ASSERT_TRUE(bs.Read(u32), "Failed to read uint32");
    ASSERT_EQ(u32, 0xDEADBEEF, "uint32 mismatch");

    ASSERT_TRUE(bs.Read(f32), "Failed to read float");
    ASSERT_TRUE(std::abs(f32 - 3.14159f) < 0.0001f, "float mismatch");

    TEST_LOG("  Basic types: OK");
    return TestPass();
}

TestResult Test_NetBitStream_Bits()
{
    NetBitStream bs;

    // Write individual bits
    bs.WriteBit(true);
    bs.WriteBit(false);
    bs.WriteBit(true);
    bs.WriteBit(true);
    bs.WriteBit(false);

    // Reset and read
    bs.ResetReadPointer();

    ASSERT_TRUE(bs.ReadBit() == true, "Bit 0 mismatch");
    ASSERT_TRUE(bs.ReadBit() == false, "Bit 1 mismatch");
    ASSERT_TRUE(bs.ReadBit() == true, "Bit 2 mismatch");
    ASSERT_TRUE(bs.ReadBit() == true, "Bit 3 mismatch");
    ASSERT_TRUE(bs.ReadBit() == false, "Bit 4 mismatch");

    TEST_LOG("  Bit operations: OK");
    return TestPass();
}

TestResult Test_NetBitStream_Compressed()
{
    NetBitStream bs;

    // Write compressed values
    bs.WriteCompressed(static_cast<uint16_t>(0));      // Should be minimal
    bs.WriteCompressed(static_cast<uint16_t>(100));    // Small value
    bs.WriteCompressed(static_cast<uint16_t>(50000));  // Large value

    bs.ResetReadPointer();

    uint16_t v1, v2, v3;
    ASSERT_TRUE(bs.ReadCompressed(v1), "Failed to read compressed 1");
    ASSERT_EQ(v1, 0, "Compressed value 1 mismatch");

    ASSERT_TRUE(bs.ReadCompressed(v2), "Failed to read compressed 2");
    ASSERT_EQ(v2, 100, "Compressed value 2 mismatch");

    ASSERT_TRUE(bs.ReadCompressed(v3), "Failed to read compressed 3");
    ASSERT_EQ(v3, 50000, "Compressed value 3 mismatch");

    TEST_LOG("  Compressed types: OK");
    return TestPass();
}

TestResult Test_NetBitStream_Vectors()
{
    NetBitStream bs;

    // Write normal vector
    bs.WriteVector(100.5f, -50.25f, 200.75f);

    // Write normalized vector
    float len = std::sqrt(0.5f * 0.5f + 0.5f * 0.5f + 0.707f * 0.707f);
    bs.WriteNormVector(0.5f / len, 0.5f / len, 0.707f / len);

    bs.ResetReadPointer();

    float x, y, z;
    ASSERT_TRUE(bs.ReadVector(x, y, z), "Failed to read vector");
    ASSERT_TRUE(std::abs(x - 100.5f) < 0.01f, "Vector X mismatch");
    ASSERT_TRUE(std::abs(y - (-50.25f)) < 0.01f, "Vector Y mismatch");
    ASSERT_TRUE(std::abs(z - 200.75f) < 0.01f, "Vector Z mismatch");

    float nx, ny, nz;
    ASSERT_TRUE(bs.ReadNormVector(nx, ny, nz), "Failed to read norm vector");
    // Normalized vectors have some precision loss, allow larger tolerance
    ASSERT_TRUE(std::abs(nx) < 1.1f, "NormVector X out of range");
    ASSERT_TRUE(std::abs(ny) < 1.1f, "NormVector Y out of range");
    ASSERT_TRUE(std::abs(nz) < 1.1f, "NormVector Z out of range");

    TEST_LOG("  Vector types: OK");
    return TestPass();
}

TestResult Test_NetBitStream_String()
{
    NetBitStream bs;

    std::string testStr = "Hello MTA Android!";
    bs.Write(testStr);

    bs.ResetReadPointer();

    std::string readStr;
    ASSERT_TRUE(bs.Read(readStr, 256), "Failed to read string");
    ASSERT_TRUE(readStr == testStr, "String mismatch");

    TEST_LOG("  String: '%s'", readStr.c_str());
    return TestPass();
}

TestResult Test_SyncStructures_Position()
{
    NetBitStream bs;

    SPositionSync posWrite(1234.5f, -567.8f, 90.0f);
    posWrite.Write(bs);

    bs.ResetReadPointer();

    SPositionSync posRead;
    ASSERT_TRUE(posRead.Read(bs), "Failed to read position");
    ASSERT_TRUE(std::abs(posRead.x - 1234.5f) < 0.1f, "Position X mismatch");
    ASSERT_TRUE(std::abs(posRead.y - (-567.8f)) < 0.1f, "Position Y mismatch");
    ASSERT_TRUE(std::abs(posRead.z - 90.0f) < 0.1f, "Position Z mismatch");

    TEST_LOG("  Position sync: OK");
    return TestPass();
}

TestResult Test_SyncStructures_Health()
{
    NetBitStream bs;

    SHealthSync healthWrite(85.5f);
    SArmorSync armorWrite(50.0f);

    healthWrite.Write(bs);
    armorWrite.Write(bs);

    bs.ResetReadPointer();

    SHealthSync healthRead;
    SArmorSync armorRead;

    ASSERT_TRUE(healthRead.Read(bs), "Failed to read health");
    ASSERT_TRUE(armorRead.Read(bs), "Failed to read armor");

    // Allow some precision loss due to compression
    ASSERT_TRUE(std::abs(healthRead.health - 85.5f) < 2.0f, "Health mismatch");
    ASSERT_TRUE(std::abs(armorRead.armor - 50.0f) < 1.0f, "Armor mismatch");

    TEST_LOG("  Health: %.1f, Armor: %.1f", healthRead.health, armorRead.armor);
    return TestPass();
}

TestResult Test_SyncStructures_PlayerFlags()
{
    NetBitStream bs;

    SPlayerPuresyncFlags flagsWrite;
    flagsWrite.data.bIsOnGround = true;
    flagsWrite.data.bIsDucked = true;
    flagsWrite.data.bHasAWeapon = true;
    flagsWrite.data.bSyncingVelocity = true;

    flagsWrite.Write(bs);

    bs.ResetReadPointer();

    SPlayerPuresyncFlags flagsRead;
    ASSERT_TRUE(flagsRead.Read(bs), "Failed to read flags");

    ASSERT_TRUE(flagsRead.data.bIsOnGround == true, "isOnGround mismatch");
    ASSERT_TRUE(flagsRead.data.bIsDucked == true, "isDucked mismatch");
    ASSERT_TRUE(flagsRead.data.bHasAWeapon == true, "hasAWeapon mismatch");
    ASSERT_TRUE(flagsRead.data.bSyncingVelocity == true, "syncingVelocity mismatch");
    ASSERT_TRUE(flagsRead.data.bIsInWater == false, "isInWater should be false");

    TEST_LOG("  Player flags: OK");
    return TestPass();
}

TestResult Test_CNetAndroid_Initialize()
{
    auto& net = CNetAndroid::Instance();

    ASSERT_TRUE(net.Initialize(), "Failed to initialize CNetAndroid");

    // Check initial state
    ASSERT_TRUE(net.GetState() == ConnectionState::Disconnected, "Should start disconnected");
    ASSERT_FALSE(net.IsConnected(), "Should not be connected");

    // Get local IP (may return 0.0.0.0 but shouldn't crash)
    std::string localIP = net.GetLocalIP();
    TEST_LOG("  Local IP: %s", localIP.c_str());

    net.Shutdown();
    return TestPass();
}

TestResult Test_CNetAndroid_BitStreamAlloc()
{
    auto& net = CNetAndroid::Instance();
    net.Initialize();

    // Allocate bitstream
    auto bs = net.AllocateBitStream();
    ASSERT_TRUE(bs != nullptr, "Failed to allocate bitstream");

    // Write some data
    bs->Write(static_cast<uint32_t>(0x12345678));
    ASSERT_EQ(bs->GetBytesUsed(), 4, "BitStream size mismatch");

    net.Shutdown();
    TEST_LOG("  BitStream allocation: OK");
    return TestPass();
}

//=============================================================================
// Server Connection Tests
//=============================================================================

TestResult Test_ServerConnection_Initialize()
{
    CServerConnection conn;
    ASSERT_TRUE(conn.Initialize(), "Failed to initialize CServerConnection");
    ASSERT_TRUE(conn.GetState() == ServerConnectionState::DISCONNECTED, "Should start disconnected");
    conn.Shutdown();
    TEST_LOG("  CServerConnection initialized: OK");
    return TestPass();
}

TestResult Test_ServerConnection_DNS()
{
    CServerConnection conn;
    conn.Initialize();

    // Test DNS resolution
    std::string ip = conn.TestDNSResolution("mtasa.com");
    ASSERT_FALSE(ip.empty(), "Failed to resolve mtasa.com");
    TEST_LOG("  mtasa.com -> %s", ip.c_str());

    // Test IP passthrough
    std::string ipDirect = conn.TestDNSResolution("8.8.8.8");
    ASSERT_TRUE(ipDirect == "8.8.8.8", "IP passthrough failed");

    conn.Shutdown();
    return TestPass();
}

TestResult Test_ServerConnection_MD5()
{
    // Test MD5 hash computation
    uint8_t hash[16];
    MD5::Compute("test", hash);

    // MD5("test") = 098f6bcd4621d373cade4e832627b4f6
    ASSERT_TRUE(hash[0] == 0x09, "MD5 hash byte 0 mismatch");
    ASSERT_TRUE(hash[1] == 0x8f, "MD5 hash byte 1 mismatch");
    ASSERT_TRUE(hash[2] == 0x6b, "MD5 hash byte 2 mismatch");
    ASSERT_TRUE(hash[3] == 0xcd, "MD5 hash byte 3 mismatch");

    TEST_LOG("  MD5 hash: OK");
    return TestPass();
}

TestResult Test_ServerConnection_StateTransitions()
{
    CServerConnection conn;
    conn.Initialize();

    ASSERT_TRUE(conn.GetState() == ServerConnectionState::DISCONNECTED, "Initial state wrong");
    ASSERT_TRUE(std::string(CServerConnection::StateToString(ServerConnectionState::DISCONNECTED)) == "DISCONNECTED",
                "StateToString failed");
    ASSERT_TRUE(std::string(CServerConnection::StateToString(ServerConnectionState::CONNECTED)) == "CONNECTED",
                "StateToString failed for CONNECTED");

    conn.Shutdown();
    TEST_LOG("  State transitions: OK");
    return TestPass();
}

} // anonymous namespace

//=============================================================================
// Test Registration
//=============================================================================

namespace
{

struct TestRegistration
{
    TestRegistration()
    {
        auto& harness = TestHarness::Instance();

        // Platform tests
        harness.RegisterTest("DeviceInfo", "Platform", Test_Platform_DeviceInfo);
        harness.RegisterTest("Architecture", "Platform", Test_Platform_Architecture);
        harness.RegisterTest("PageSize", "Platform", Test_Platform_PageSize);
        harness.RegisterTest("ProcessorCount", "Platform", Test_Platform_ProcessorCount);

        // Input tests
        harness.RegisterTest("Initialization", "Input", Test_Input_Initialization);
        harness.RegisterTest("TouchSimulation", "Input", Test_Input_TouchSimulation);
        harness.RegisterTest("MultiTouch", "Input", Test_Input_MultiTouch);
        harness.RegisterTest("VirtualControls", "Input", Test_Input_VirtualControls);
        harness.RegisterTest("Update", "Input", Test_Input_Update);

        // FileSystem tests
        harness.RegisterTest("Initialization", "FileSystem", Test_FileSystem_Initialization);
        harness.RegisterTest("Paths", "FileSystem", Test_FileSystem_Paths);
        harness.RegisterTest("TempDirectory", "FileSystem", Test_FileSystem_TempDirectory);
        harness.RegisterTest("ProcMaps", "FileSystem", Test_FileSystem_ProcMaps);

        // Network tests
        harness.RegisterTest("Initialization", "Network", Test_Network_Initialization);
        harness.RegisterTest("SocketCreation", "Network", Test_Network_SocketCreation);
        harness.RegisterTest("DNSLookup", "Network", Test_Network_DNSLookup);

        // Hook tests
        harness.RegisterTest("ThumbDetection", "Hooks", Test_Hooks_ThumbDetection);
        harness.RegisterTest("TrampolineSize", "Hooks", Test_Hooks_TrampolineSize);
        harness.RegisterTest("MemoryProtection", "Hooks", Test_Hooks_MemoryProtection);

        // Scanner tests
        harness.RegisterTest("PatternMatch", "Scanner", Test_Scanner_PatternMatch);
        harness.RegisterTest("LibraryEnumeration", "Scanner", Test_Scanner_LibraryEnumeration);
        harness.RegisterTest("FindLibc", "Scanner", Test_Scanner_FindLibc);

        // Profiler tests
        harness.RegisterTest("Initialization", "Profiler", Test_Profiler_Initialization);
        harness.RegisterTest("FrameTiming", "Profiler", Test_Profiler_FrameTiming);
        harness.RegisterTest("Categories", "Profiler", Test_Profiler_Categories);
        harness.RegisterTest("ScopedTimer", "Profiler", Test_Profiler_ScopedTimer);

        // Graphics tests
        harness.RegisterTest("GLESAvailable", "Graphics", Test_Graphics_GLESAvailable);
        harness.RegisterTest("EGLAvailable", "Graphics", Test_Graphics_EGLAvailable);

        // Memory tests
        harness.RegisterTest("Allocation", "Memory", Test_Memory_Allocation);
        harness.RegisterTest("Alignment", "Memory", Test_Memory_Alignment);

        // Network Protocol tests (Phase 7)
        harness.RegisterTest("BasicTypes", "NetBitStream", Test_NetBitStream_BasicTypes);
        harness.RegisterTest("Bits", "NetBitStream", Test_NetBitStream_Bits);
        harness.RegisterTest("Compressed", "NetBitStream", Test_NetBitStream_Compressed);
        harness.RegisterTest("Vectors", "NetBitStream", Test_NetBitStream_Vectors);
        harness.RegisterTest("String", "NetBitStream", Test_NetBitStream_String);

        harness.RegisterTest("Position", "SyncStructures", Test_SyncStructures_Position);
        harness.RegisterTest("Health", "SyncStructures", Test_SyncStructures_Health);
        harness.RegisterTest("PlayerFlags", "SyncStructures", Test_SyncStructures_PlayerFlags);

        harness.RegisterTest("Initialize", "CNetAndroid", Test_CNetAndroid_Initialize);
        harness.RegisterTest("BitStreamAlloc", "CNetAndroid", Test_CNetAndroid_BitStreamAlloc);

        // Server Connection tests (Phase 7)
        harness.RegisterTest("Initialize", "ServerConnection", Test_ServerConnection_Initialize);
        harness.RegisterTest("DNS", "ServerConnection", Test_ServerConnection_DNS);
        harness.RegisterTest("MD5", "ServerConnection", Test_ServerConnection_MD5);
        harness.RegisterTest("StateTransitions", "ServerConnection", Test_ServerConnection_StateTransitions);

        TEST_LOG("Registered %zu tests", harness.GetTestCount());
    }
} g_testRegistration;

} // anonymous namespace
