/*
 * MTA:SA Android - Game SA Platform Implementation
 *
 * Platform-specific initialization and utilities for the game_sa layer.
 */

#include "GameSA_Platform.h"

#ifdef PLATFORM_ARM

#include <dlfcn.h>
#include <cstring>
#include <cstdio>

#ifdef __ANDROID__
#include <android/log.h>
#define PLATFORM_LOG_TAG "MTA-GameSA"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, PLATFORM_LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, PLATFORM_LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, PLATFORM_LOG_TAG, __VA_ARGS__)
#else
#define LOGI(...) printf(__VA_ARGS__)
#define LOGE(...) fprintf(stderr, __VA_ARGS__)
#define LOGD(...) printf(__VA_ARGS__)
#endif

//=============================================================================
// Global Variables
//=============================================================================

// Base address of libGTASA.so
uintptr_t g_GTASABase = 0;

// Handle to libGTASA.so
static void* s_hGTASA = nullptr;

// Flag to track initialization
static bool s_bInitialized = false;

//=============================================================================
// Library Information
//=============================================================================

struct LibraryInfo
{
    uintptr_t base;
    uintptr_t end;
    char      name[256];
};

/**
 * Find library base address by parsing /proc/self/maps
 */
static bool FindLibraryInfo(const char* libName, LibraryInfo* outInfo)
{
    FILE* fp = fopen("/proc/self/maps", "r");
    if (!fp)
    {
        LOGE("Failed to open /proc/self/maps");
        return false;
    }

    char line[512];
    bool found = false;

    while (fgets(line, sizeof(line), fp))
    {
        if (strstr(line, libName))
        {
            // Parse the line: start-end perms offset dev inode pathname
            uintptr_t start, end;
            char perms[5];

            if (sscanf(line, "%lx-%lx %4s", &start, &end, perms) == 3)
            {
                // Only care about executable sections
                if (perms[2] == 'x')
                {
                    if (!found)
                    {
                        outInfo->base = start;
                        found = true;
                    }
                    outInfo->end = end;
                }
            }
        }
    }

    fclose(fp);

    if (found)
    {
        strncpy(outInfo->name, libName, sizeof(outInfo->name) - 1);
        outInfo->name[sizeof(outInfo->name) - 1] = '\0';
        LOGI("Found %s: base=0x%lx, end=0x%lx, size=%lu KB",
             libName, outInfo->base, outInfo->end,
             (outInfo->end - outInfo->base) / 1024);
    }

    return found;
}

//=============================================================================
// Platform Initialization
//=============================================================================

bool InitializeGamePlatform(uintptr_t gtasaBase)
{
    if (s_bInitialized)
        return true;

    LOGI("Initializing Game SA Platform for ARM...");

    // If base address is provided, use it
    if (gtasaBase != 0)
    {
        g_GTASABase = gtasaBase;
        LOGI("Using provided GTASA base: 0x%lx", g_GTASABase);
    }
    else
    {
        // Try to find libGTASA.so
        LibraryInfo gtasaInfo = {};

        // Try different possible library names
        const char* possibleNames[] = {
            "libGTASA.so",
            "libGtaSa.so",
            "libgtasa.so",
            "GTASA.so",
            nullptr
        };

        bool found = false;
        for (const char** name = possibleNames; *name != nullptr; name++)
        {
            if (FindLibraryInfo(*name, &gtasaInfo))
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            LOGE("Failed to find GTA:SA library in memory");
            return false;
        }

        g_GTASABase = gtasaInfo.base;
    }

    LOGI("GTA:SA base address: 0x%lx", g_GTASABase);

    // Validate the base address by checking for expected patterns
    // This helps ensure we have the correct library

    // Try to open the library for symbol resolution
    s_hGTASA = dlopen("libGTASA.so", RTLD_NOLOAD);
    if (s_hGTASA)
    {
        LOGI("Got handle to libGTASA.so");
    }
    else
    {
        LOGD("Could not get library handle (this is okay): %s", dlerror());
    }

    s_bInitialized = true;
    LOGI("Game SA Platform initialized successfully");

    return true;
}

void ShutdownGamePlatform()
{
    if (!s_bInitialized)
        return;

    LOGI("Shutting down Game SA Platform...");

    if (s_hGTASA)
    {
        // Don't actually close the library
        s_hGTASA = nullptr;
    }

    g_GTASABase = 0;
    s_bInitialized = false;

    LOGI("Game SA Platform shutdown complete");
}

//=============================================================================
// Symbol Resolution
//=============================================================================

/**
 * Try to resolve a symbol from libGTASA.so
 * This is useful for exported functions
 */
void* ResolveGameSymbol(const char* symbolName)
{
    if (!s_hGTASA)
    {
        // Try to open the library
        s_hGTASA = dlopen("libGTASA.so", RTLD_NOLOAD);
        if (!s_hGTASA)
            return nullptr;
    }

    return dlsym(s_hGTASA, symbolName);
}

//=============================================================================
// Memory Validation
//=============================================================================

/**
 * Check if an address is within the GTA:SA address space
 */
bool IsValidGameAddress(uintptr_t address)
{
    if (g_GTASABase == 0)
        return false;

    // Assuming the library is less than 100MB
    const uintptr_t maxSize = 100 * 1024 * 1024;

    return (address >= g_GTASABase && address < g_GTASABase + maxSize);
}

/**
 * Validate a function pointer before calling
 */
bool ValidateFunctionPointer(uintptr_t funcPtr)
{
    if (!IsValidGameAddress(funcPtr))
    {
        LOGE("Invalid function pointer: 0x%lx (base: 0x%lx)", funcPtr, g_GTASABase);
        return false;
    }

#ifdef PLATFORM_ARM32
    // On ARM32, Thumb functions have bit 0 set
    // Check that the address makes sense
    uintptr_t cleanAddr = funcPtr & ~1;
    if (cleanAddr == 0)
    {
        LOGE("Null function pointer after cleaning: 0x%lx", funcPtr);
        return false;
    }
#endif

    return true;
}

//=============================================================================
// Debug Utilities
//=============================================================================

/**
 * Dump information about the game platform
 */
void DumpPlatformInfo()
{
    LOGI("=== Game SA Platform Info ===");
    LOGI("  Initialized: %s", s_bInitialized ? "yes" : "no");
    LOGI("  GTASA Base: 0x%lx", g_GTASABase);

#ifdef PLATFORM_ARM32
    LOGI("  Architecture: ARM32");
#elif defined(PLATFORM_ARM64)
    LOGI("  Architecture: ARM64");
#endif

    LOGI("  Library Handle: %p", s_hGTASA);

    // Print some key addresses
    LOGI("  Key Addresses:");
    LOGI("    CPed_ProcessControl: 0x%lx", GetGameAddress(GameAddr::CPed_ProcessControl));
    LOGI("    CVehicle_SetupRender: 0x%lx", GetGameAddress(GameAddr::CVehicle_SetupRender));
    LOGI("    CWorld_ProcessLineOfSight: 0x%lx", GetGameAddress(GameAddr::CWorld_ProcessLineOfSight));
    LOGI("==============================");
}

#endif // PLATFORM_ARM
