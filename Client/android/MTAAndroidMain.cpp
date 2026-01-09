/*
 * MTA:SA Android - Main Entry Point
 *
 * This is the main native library that gets loaded into the GTA:SA Android process.
 * It initializes the hook system, signature scanner, and MTA modules.
 */

#include <jni.h>
#include <android/log.h>
#include <dlfcn.h>
#include <cstdio>
#include <cstring>

#include "hooks/ARMHookSystem.h"
#include "signatures/SignatureScanner.h"
#include "game_sa/GTASAIntegration.h"

// =============================================================================
// Logging
// =============================================================================

#define LOG_TAG "MTA:SA"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

// =============================================================================
// Global State
// =============================================================================

namespace MTA::Android
{
    // GTA:SA library info
    struct LibraryInfo
    {
        void*       handle;
        uintptr_t   base;
        size_t      size;
        bool        loaded;

        LibraryInfo() : handle(nullptr), base(0), size(0), loaded(false) {}
    };

    static LibraryInfo g_gtasaLib;
    static bool g_initialized = false;

    // =============================================================================
    // Library Detection
    // =============================================================================

    /**
     * Find loaded library by name and get its base address
     * Parses /proc/self/maps to find memory mappings
     */
    bool FindLibrary(const char* libName, LibraryInfo& info)
    {
        char line[512];
        FILE* fp = fopen("/proc/self/maps", "r");
        if (!fp)
        {
            LOGE("Failed to open /proc/self/maps");
            return false;
        }

        uintptr_t startAddr = 0;
        uintptr_t endAddr = 0;
        bool found = false;

        while (fgets(line, sizeof(line), fp))
        {
            if (strstr(line, libName))
            {
                // Parse line format: "start-end perms offset dev inode pathname"
                uintptr_t start, end;
                if (sscanf(line, "%lx-%lx", &start, &end) == 2)
                {
                    if (!found)
                    {
                        startAddr = start;
                        found = true;
                    }
                    endAddr = end;  // Keep updating to get full range
                }
            }
        }

        fclose(fp);

        if (found)
        {
            info.base = startAddr;
            info.size = endAddr - startAddr;
            info.loaded = true;
            LOGI("Found %s at 0x%lX - 0x%lX (size: %zu bytes)",
                 libName, startAddr, endAddr, info.size);
            return true;
        }

        LOGW("Library %s not found in memory maps", libName);
        return false;
    }

    /**
     * Wait for GTA:SA library to be loaded
     * Called periodically until the library is found
     */
    bool WaitForGTASA()
    {
        // Common GTA:SA library names on Android
        const char* libNames[] = {
            "libGTASA.so",
            "libgtasa.so",
            "libGTASAUnity.so",  // Just in case
            nullptr
        };

        for (int i = 0; libNames[i] != nullptr; i++)
        {
            if (FindLibrary(libNames[i], g_gtasaLib))
            {
                return true;
            }
        }

        return false;
    }

    // =============================================================================
    // Initialization
    // =============================================================================

    /**
     * Initialize signature scanner with GTA:SA memory region
     */
    bool InitializeScanner(Signatures::SignatureScanner& scanner)
    {
        if (!g_gtasaLib.loaded)
        {
            LOGE("Cannot initialize scanner: GTA:SA not loaded");
            return false;
        }

        scanner.AddRegion(g_gtasaLib.base, g_gtasaLib.size, "libGTASA.so");
        LOGI("Scanner initialized with region: 0x%lX, size: %zu",
             g_gtasaLib.base, g_gtasaLib.size);

        return true;
    }

    /**
     * Resolve all known signatures
     */
    bool ResolveSignatures()
    {
        Signatures::SignatureScanner scanner;
        if (!InitializeScanner(scanner))
        {
            return false;
        }

        // Register known GTA:SA signatures
        Signatures::RegisterGTASASignatures();

        // Resolve addresses
        auto& mapper = Signatures::AddressMapper::Instance();
        size_t resolved = mapper.ResolveAll(scanner);

        size_t total, resolvedCount, verified;
        mapper.GetStats(total, resolvedCount, verified);

        LOGI("Signature resolution: %zu/%zu resolved (%zu verified)",
             resolvedCount, total, verified);

        // Export mapping for debugging
        std::string json = mapper.ExportJSON();
        LOGD("Address mappings:\n%s", json.c_str());

        return resolvedCount > 0;
    }

    /**
     * Install all hooks
     */
    bool InstallHooks()
    {
        auto& hookMgr = Hooks::HookManager::Instance();
        auto& mapper = Signatures::AddressMapper::Instance();

        // Example: Install a test hook (disabled for now)
        /*
        uintptr_t entityRender = mapper.GetARMAddress("CEntity::Render");
        if (entityRender != 0)
        {
            if (hookMgr.Install("CEntity::Render", entityRender, (uintptr_t)&Hook_CEntity_Render))
            {
                LOGI("Installed hook: CEntity::Render at 0x%lX", entityRender);
            }
        }
        */

        LOGI("Hook installation complete");
        return true;
    }

    /**
     * Main initialization function
     */
    bool Initialize()
    {
        if (g_initialized)
        {
            LOGW("Already initialized");
            return true;
        }

        LOGI("===========================================");
        LOGI("MTA:SA Android v1.6.0 - Initializing");
        LOGI("===========================================");

        // Wait for GTA:SA to load
        if (!WaitForGTASA())
        {
            LOGE("GTA:SA library not found - will retry later");
            return false;
        }

        // Resolve signatures
        if (!ResolveSignatures())
        {
            LOGW("Some signatures failed to resolve - continuing anyway");
        }

        // Install hooks
        if (!InstallHooks())
        {
            LOGE("Failed to install hooks");
            return false;
        }

        // Initialize GTA:SA integration with God Mode enabled (proof-of-concept)
        if (GTASA::InitializeWithGodMode())
        {
            LOGI("GTA:SA integration with God Mode initialized!");
        }
        else
        {
            LOGW("GTA:SA integration failed - continuing without it");
        }

        g_initialized = true;
        LOGI("MTA:SA Android initialized successfully!");
        return true;
    }

    /**
     * Cleanup function
     */
    void Shutdown()
    {
        if (!g_initialized) return;

        LOGI("MTA:SA Android shutting down...");

        // Uninstall all hooks
        Hooks::HookManager::Instance().UninstallAll();

        g_initialized = false;
        LOGI("Shutdown complete");
    }

} // namespace MTA::Android

// =============================================================================
// JNI Entry Points
// Note: JNI_OnLoad and JNI_OnUnload are defined in jni/MTANative.cpp
// =============================================================================

extern "C" {

/**
 * Manual initialization from Java (if JNI_OnLoad is too early)
 */
JNIEXPORT jboolean JNICALL
Java_com_mtasa_android_MTANative_initialize(JNIEnv* env, jclass clazz)
{
    return MTA::Android::Initialize() ? JNI_TRUE : JNI_FALSE;
}

/**
 * Check if MTA is initialized
 */
JNIEXPORT jboolean JNICALL
Java_com_mtasa_android_MTANative_isInitialized(JNIEnv* env, jclass clazz)
{
    return MTA::Android::g_initialized ? JNI_TRUE : JNI_FALSE;
}

/**
 * Get version string
 */
JNIEXPORT jstring JNICALL
Java_com_mtasa_android_MTANative_getVersion(JNIEnv* env, jclass clazz)
{
    return env->NewStringUTF("MTA:SA Android 1.6.0-alpha");
}

/**
 * Get address mapping as JSON
 */
JNIEXPORT jstring JNICALL
Java_com_mtasa_android_MTANative_getAddressMappings(JNIEnv* env, jclass clazz)
{
    std::string json = MTA::Android::Signatures::AddressMapper::Instance().ExportJSON();
    return env->NewStringUTF(json.c_str());
}

} // extern "C"
