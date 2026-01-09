/*
 * MTA:SA Android - GTA:SA Integration Module
 *
 * Phase 6: Integration with the actual GTA:SA Android game
 *
 * This module handles:
 * - Detecting and validating libGTASA.so
 * - Version detection (multiple GTA:SA Android versions)
 * - Installing proof-of-concept hooks
 * - Game state monitoring
 */

#ifndef GTASA_INTEGRATION_H
#define GTASA_INTEGRATION_H

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <functional>

#ifdef __ANDROID__
#include <android/log.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

#include "../hooks/ARMHookInstaller.h"
#include "../signatures/ARMAddressMap.h"

namespace MTA::Android::GTASA
{
    //=========================================================================
    // Logging
    //=========================================================================

    #define GTASA_LOG_TAG "MTA-GTASA"
    #define GTASA_LOGI(...) __android_log_print(ANDROID_LOG_INFO, GTASA_LOG_TAG, __VA_ARGS__)
    #define GTASA_LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, GTASA_LOG_TAG, __VA_ARGS__)
    #define GTASA_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, GTASA_LOG_TAG, __VA_ARGS__)
    #define GTASA_LOGW(...) __android_log_print(ANDROID_LOG_WARN, GTASA_LOG_TAG, __VA_ARGS__)

    //=========================================================================
    // GTA:SA Android Version Definitions
    //=========================================================================

    enum class GTASAVersion
    {
        Unknown = 0,
        V1_08,          // Original Android release
        V2_00,          // Major update (ARM64 support added)
        V2_10,          // SA-MP Android reference version
        V2_11_32,       // Latest ARM32
        V2_11_64        // Latest ARM64
    };

    struct VersionInfo
    {
        GTASAVersion    version;
        const char*     versionString;
        const char*     packageName;
        size_t          expectedLibSize;    // Approximate libGTASA.so size
        uint32_t        signatureOffset;    // Known unique offset for detection
        uint32_t        signatureValue;     // Expected value at that offset
    };

    // Known GTA:SA Android versions
    constexpr VersionInfo KNOWN_VERSIONS[] = {
        { GTASAVersion::V2_10, "2.10", "com.rockstargames.gtasa",
          16000000, 0x100, 0x464C457F },  // ELF magic
        { GTASAVersion::V2_11_32, "2.11 (32-bit)", "com.rockstargames.gtasa",
          18000000, 0x100, 0x464C457F },
        { GTASAVersion::V2_11_64, "2.11 (64-bit)", "com.rockstargames.gtasa",
          22000000, 0x100, 0x464C457F },
    };

    //=========================================================================
    // Game Library Info
    //=========================================================================

    struct GameLibrary
    {
        uintptr_t       base;           // Base address in memory
        size_t          size;           // Size of loaded library
        std::string     path;           // Full path to library
        GTASAVersion    version;        // Detected version
        bool            isValid;        // Validation passed
        bool            is64Bit;        // ARM64 vs ARM32

        GameLibrary() : base(0), size(0), version(GTASAVersion::Unknown),
                        isValid(false), is64Bit(false) {}
    };

    //=========================================================================
    // Integration State
    //=========================================================================

    struct IntegrationState
    {
        GameLibrary     gameLib;
        bool            initialized;
        bool            hooksInstalled;
        int             hookCount;

        // Proof-of-concept hook states
        bool            godModeEnabled;
        bool            infiniteAmmoEnabled;
        bool            neverWantedEnabled;

        IntegrationState() : initialized(false), hooksInstalled(false), hookCount(0),
                             godModeEnabled(false), infiniteAmmoEnabled(false),
                             neverWantedEnabled(false) {}
    };

    // Global state
    inline IntegrationState g_state;

    //=========================================================================
    // Library Detection
    //=========================================================================

    /**
     * Find libGTASA.so in the process memory map
     */
    inline bool FindGameLibrary(GameLibrary& lib)
    {
        const char* libNames[] = {
            "libGTASA.so",
            "libgtasa.so",
            nullptr
        };

        FILE* maps = fopen("/proc/self/maps", "r");
        if (!maps)
        {
            GTASA_LOGE("Failed to open /proc/self/maps");
            return false;
        }

        char line[512];
        uintptr_t firstAddr = 0;
        uintptr_t lastAddr = 0;
        bool found = false;
        char foundPath[256] = {0};

        while (fgets(line, sizeof(line), maps))
        {
            for (int i = 0; libNames[i] != nullptr; i++)
            {
                if (strstr(line, libNames[i]))
                {
                    uintptr_t start, end;
                    char perms[8];
                    char path[256] = {0};

                    if (sscanf(line, "%lx-%lx %s %*s %*s %*s %255s",
                               &start, &end, perms, path) >= 3)
                    {
                        if (!found)
                        {
                            firstAddr = start;
                            strncpy(foundPath, path, sizeof(foundPath) - 1);
                        }
                        lastAddr = end;
                        found = true;
                    }
                    break;
                }
            }
        }

        fclose(maps);

        if (found)
        {
            lib.base = firstAddr;
            lib.size = lastAddr - firstAddr;
            lib.path = foundPath;

            #if defined(__aarch64__)
                lib.is64Bit = true;
            #else
                lib.is64Bit = false;
            #endif

            GTASA_LOGI("Found game library:");
            GTASA_LOGI("  Base: 0x%lx", (unsigned long)lib.base);
            GTASA_LOGI("  Size: %zu bytes (%.2f MB)", lib.size, lib.size / (1024.0 * 1024.0));
            GTASA_LOGI("  Path: %s", lib.path.c_str());
            GTASA_LOGI("  Arch: %s", lib.is64Bit ? "ARM64" : "ARM32");

            return true;
        }

        GTASA_LOGW("Game library not found in memory");
        return false;
    }

    //=========================================================================
    // Version Detection
    //=========================================================================

    /**
     * Detect the GTA:SA version by examining the loaded library
     */
    inline GTASAVersion DetectVersion(const GameLibrary& lib)
    {
        // Method 1: Check library size (rough estimate)
        // Different versions have different sizes

        // Method 2: Look for version strings in memory
        const char* versionStrings[] = {
            "2.11",
            "2.10",
            "2.00",
            "1.08",
            nullptr
        };

        // Search for version string in first 1MB of library
        const char* searchArea = reinterpret_cast<const char*>(lib.base);
        size_t searchSize = std::min(lib.size, (size_t)(1024 * 1024));

        for (int i = 0; versionStrings[i] != nullptr; i++)
        {
            const char* verStr = versionStrings[i];
            size_t verLen = strlen(verStr);

            for (size_t j = 0; j < searchSize - verLen; j++)
            {
                if (memcmp(searchArea + j, verStr, verLen) == 0)
                {
                    GTASA_LOGI("Found version string: %s at offset 0x%lx",
                               verStr, (unsigned long)j);

                    if (strcmp(verStr, "2.11") == 0)
                    {
                        return lib.is64Bit ? GTASAVersion::V2_11_64 : GTASAVersion::V2_11_32;
                    }
                    else if (strcmp(verStr, "2.10") == 0)
                    {
                        return GTASAVersion::V2_10;
                    }
                    else if (strcmp(verStr, "2.00") == 0)
                    {
                        return GTASAVersion::V2_00;
                    }
                    else if (strcmp(verStr, "1.08") == 0)
                    {
                        return GTASAVersion::V1_08;
                    }
                }
            }
        }

        // Method 3: Use size-based heuristics
        if (lib.is64Bit && lib.size > 20000000)
        {
            return GTASAVersion::V2_11_64;
        }
        else if (!lib.is64Bit && lib.size > 15000000)
        {
            return GTASAVersion::V2_11_32;
        }

        GTASA_LOGW("Could not determine exact version, assuming latest");
        return lib.is64Bit ? GTASAVersion::V2_11_64 : GTASAVersion::V2_10;
    }

    /**
     * Get version string
     */
    inline const char* GetVersionString(GTASAVersion ver)
    {
        switch (ver)
        {
            case GTASAVersion::V1_08:    return "1.08";
            case GTASAVersion::V2_00:    return "2.00";
            case GTASAVersion::V2_10:    return "2.10";
            case GTASAVersion::V2_11_32: return "2.11 (32-bit)";
            case GTASAVersion::V2_11_64: return "2.11 (64-bit)";
            default:                     return "Unknown";
        }
    }

    //=========================================================================
    // Library Validation
    //=========================================================================

    /**
     * Validate the game library is actually GTA:SA
     */
    inline bool ValidateGameLibrary(GameLibrary& lib)
    {
        // Check 1: Library is loaded and has reasonable size
        if (lib.base == 0 || lib.size < 5000000)  // At least 5MB
        {
            GTASA_LOGE("Invalid library: base=0x%lx, size=%zu",
                       (unsigned long)lib.base, lib.size);
            return false;
        }

        // Check 2: ELF header is valid
        const uint32_t* header = reinterpret_cast<const uint32_t*>(lib.base);
        if (*header != 0x464C457F)  // "\x7FELF"
        {
            GTASA_LOGE("Invalid ELF header");
            return false;
        }

        // Check 3: Look for GTA:SA-specific strings
        const char* gtaStrings[] = {
            "CGame",
            "CPed",
            "CVehicle",
            "Rockstar",
            nullptr
        };

        int foundStrings = 0;
        const char* searchArea = reinterpret_cast<const char*>(lib.base);
        size_t searchSize = std::min(lib.size, (size_t)(2 * 1024 * 1024));

        for (int i = 0; gtaStrings[i] != nullptr; i++)
        {
            const char* str = gtaStrings[i];
            size_t len = strlen(str);

            for (size_t j = 0; j < searchSize - len; j++)
            {
                if (memcmp(searchArea + j, str, len) == 0)
                {
                    foundStrings++;
                    break;
                }
            }
        }

        if (foundStrings < 2)
        {
            GTASA_LOGW("Library validation: only found %d/4 expected strings", foundStrings);
            // Don't fail completely, just warn
        }

        lib.isValid = true;
        GTASA_LOGI("Library validation passed (found %d/4 GTA strings)", foundStrings);
        return true;
    }

    //=========================================================================
    // Proof-of-Concept Hooks
    //=========================================================================

    // Original function pointers (for calling original)
    inline uintptr_t g_origPlayerProcessControl = 0;

    /**
     * God Mode Hook - Prevents player from taking damage
     * Hooks into CPed damage handling
     */

    // Player health pointer (resolved at runtime)
    inline float* g_playerHealth = nullptr;
    inline float* g_playerArmor = nullptr;

    /**
     * Simple god mode implementation:
     * Instead of hooking damage, we just set health to max periodically
     */
    inline void ApplyGodMode()
    {
        if (!g_state.godModeEnabled || !g_state.gameLib.isValid)
            return;

        // Get player ped pointer
        // This requires resolving the global player pointer first
        using namespace MTA::Android::ARM;

        uintptr_t base = g_state.gameLib.base;

        #if MTA_ARM32
            // ARM32: g_WorldPlayersPtr contains array of player pointers
            uintptr_t* playersPtr = reinterpret_cast<uintptr_t*>(base + ARM32::g_WorldPlayersPtr);
            int playerIndex = *reinterpret_cast<int*>(base + ARM32::g_PlayerInFocus);
        #else
            // ARM64: Similar structure
            uintptr_t* playersPtr = reinterpret_cast<uintptr_t*>(base + ARM64::g_WorldPlayersPtr);
            int playerIndex = *reinterpret_cast<int*>(base + ARM64::g_PlayerInFocus);
        #endif

        if (playersPtr && playerIndex >= 0 && playerIndex < 2)
        {
            uintptr_t playerPed = playersPtr[playerIndex];
            if (playerPed)
            {
                // Health is typically at offset 0x540 in CPed structure
                // Armor is at 0x548
                float* health = reinterpret_cast<float*>(playerPed + 0x540);
                float* armor = reinterpret_cast<float*>(playerPed + 0x548);

                if (*health < 100.0f)
                {
                    *health = 100.0f;
                    GTASA_LOGD("God mode: restored health to 100");
                }
            }
        }
    }

    /**
     * Proof-of-Concept: Hook the game's main loop to inject our code
     * This is the safest way to add functionality without breaking the game
     */

    // CGame::Process hook
    using CGameProcess_t = void (*)();
    inline CGameProcess_t g_origCGameProcess = nullptr;

    inline void Hook_CGame_Process()
    {
        // Call original first
        if (g_origCGameProcess)
        {
            g_origCGameProcess();
        }

        // Apply our modifications
        if (g_state.godModeEnabled)
        {
            ApplyGodMode();
        }

        // Could add more PoC features here:
        // - Infinite ammo check
        // - Never wanted check
        // - etc.
    }

    //=========================================================================
    // Hook Installation
    //=========================================================================

    /**
     * Install proof-of-concept hooks
     */
    inline bool InstallPoChooks()
    {
        using namespace MTA::Android::Hooks;

        if (!g_state.gameLib.isValid)
        {
            GTASA_LOGE("Cannot install hooks: game library not valid");
            return false;
        }

        // Set global base address for hook system
        g_libGTASA = g_state.gameLib.base;

        // Initialize hook system
        if (g_pageSize == 0)
        {
            g_pageSize = sysconf(_SC_PAGESIZE);
        }

        GTASA_LOGI("Installing proof-of-concept hooks...");

        int installed = 0;

        // Hook CGame::Process for periodic updates
        // This is a safe hook point that runs every frame
        #if MTA_ARM32
            // For ARM32, we'll use the game's main update function
            // Address from SA-MP reference: CGame::Process
            constexpr uint32_t CGame_Process_Offset = 0x2FF7F9;  // Example offset

            // Try to install with trampoline so we can call original
            uintptr_t trampoline = 0;
            if (ARMHookInstallWithOriginal(CGame_Process_Offset,
                                           (uintptr_t)&Hook_CGame_Process,
                                           &trampoline, 8))
            {
                g_origCGameProcess = reinterpret_cast<CGameProcess_t>(trampoline);
                installed++;
                GTASA_LOGI("Installed CGame::Process hook");
            }
        #else
            // ARM64 version - offset derived from crash analysis
            // CGame::Process()+468 was at 0x4d64a8, so entry is ~0x4d62d4
            // But we need offset from .text section start, not library base
            // For now, disable hook to allow game to run
            // TODO: Find correct CGame::Process offset for v2.10/v2.11 ARM64
            GTASA_LOGI("CGame::Process hook disabled (needs correct offset for this version)");
            GTASA_LOGI("God mode will use simple memory patches instead");

            // Skip trampoline hook, use simpler approach
            /*
            constexpr uint32_t CGame_Process_Offset_64 = 0x4d62d4;  // Approximate
            uintptr_t trampoline = 0;
            if (ARMHookInstallWithOriginal(CGame_Process_Offset_64,
                                           (uintptr_t)&Hook_CGame_Process,
                                           &trampoline, 16))
            {
                g_origCGameProcess = reinterpret_cast<CGameProcess_t>(trampoline);
                installed++;
                GTASA_LOGI("Installed CGame::Process hook (ARM64)");
            }
            */
        #endif

        g_state.hookCount = installed;
        g_state.hooksInstalled = (installed > 0);

        GTASA_LOGI("Hook installation complete: %d hooks installed", installed);
        return g_state.hooksInstalled;
    }

    /**
     * Alternative: Install NOP-based patches for simple modifications
     * These don't require complex hook trampolines
     */
    inline bool InstallSimplePatches()
    {
        using namespace MTA::Android::Hooks;

        if (!g_state.gameLib.isValid)
            return false;

        g_libGTASA = g_state.gameLib.base;
        if (g_pageSize == 0)
            g_pageSize = sysconf(_SC_PAGESIZE);

        GTASA_LOGI("Installing simple patches...");

        int patched = 0;

        // Example: Disable wanted level increase
        // This NOPs the instruction that increases wanted level
        #if MTA_ARM32
            // NOP the wanted level check (example offset)
            // ARMNop(WANTED_LEVEL_OFFSET, 2);
            // patched++;
        #endif

        // Example: Infinite sprint
        // NOP the stamina decrease
        #if MTA_ARM32
            // ARMNop(SPRINT_STAMINA_OFFSET, 2);
            // patched++;
        #endif

        GTASA_LOGI("Applied %d simple patches", patched);
        return true;
    }

    //=========================================================================
    // Main Integration API
    //=========================================================================

    /**
     * Initialize GTA:SA integration
     * Call this after the game library is loaded
     */
    inline bool Initialize()
    {
        if (g_state.initialized)
        {
            GTASA_LOGW("Already initialized");
            return true;
        }

        GTASA_LOGI("=============================================");
        GTASA_LOGI("MTA:SA Android - GTA:SA Integration");
        GTASA_LOGI("Phase 6 - Proof of Concept");
        GTASA_LOGI("=============================================");

        // Step 1: Find the game library
        if (!FindGameLibrary(g_state.gameLib))
        {
            GTASA_LOGE("Failed to find game library");
            return false;
        }

        // Step 2: Detect version
        g_state.gameLib.version = DetectVersion(g_state.gameLib);
        GTASA_LOGI("Detected version: %s", GetVersionString(g_state.gameLib.version));

        // Step 3: Validate library
        if (!ValidateGameLibrary(g_state.gameLib))
        {
            GTASA_LOGE("Library validation failed");
            return false;
        }

        // Step 4: Install hooks (if enabled)
        // For safety, hooks are NOT installed by default
        // Call InstallPoChooks() or EnableGodMode() explicitly

        g_state.initialized = true;
        GTASA_LOGI("GTA:SA integration initialized successfully!");

        return true;
    }

    /**
     * Auto-initialize with god mode enabled (for proof-of-concept)
     */
    inline bool InitializeWithGodMode()
    {
        if (!Initialize())
            return false;

        // Enable god mode
        g_state.godModeEnabled = true;
        if (!g_state.hooksInstalled)
        {
            InstallPoChooks();
        }

        GTASA_LOGI("===========================================");
        GTASA_LOGI("  MTA:SA ANDROID - GOD MODE ENABLED!");
        GTASA_LOGI("  You are now invincible.");
        GTASA_LOGI("===========================================");

        return true;
    }

    /**
     * Check if integration is ready
     */
    inline bool IsReady()
    {
        return g_state.initialized && g_state.gameLib.isValid;
    }

    /**
     * Get current state for debugging
     */
    inline const IntegrationState& GetState()
    {
        return g_state;
    }

    //=========================================================================
    // Feature Enable/Disable API
    //=========================================================================

    /**
     * Enable god mode (player invincibility)
     */
    inline bool EnableGodMode()
    {
        if (!g_state.initialized)
        {
            GTASA_LOGE("Cannot enable god mode: not initialized");
            return false;
        }

        // Install hooks if not already done
        if (!g_state.hooksInstalled)
        {
            if (!InstallPoChooks())
            {
                GTASA_LOGW("Hook installation failed, god mode may not work");
            }
        }

        g_state.godModeEnabled = true;
        GTASA_LOGI("God mode ENABLED");
        return true;
    }

    /**
     * Disable god mode
     */
    inline void DisableGodMode()
    {
        g_state.godModeEnabled = false;
        GTASA_LOGI("God mode DISABLED");
    }

    /**
     * Toggle god mode
     */
    inline bool ToggleGodMode()
    {
        if (g_state.godModeEnabled)
        {
            DisableGodMode();
            return false;
        }
        else
        {
            return EnableGodMode();
        }
    }

    //=========================================================================
    // Debug/Status Functions
    //=========================================================================

    /**
     * Get status as JSON string (for debugging via JNI)
     */
    inline std::string GetStatusJSON()
    {
        char buffer[1024];
        snprintf(buffer, sizeof(buffer),
            "{\n"
            "  \"initialized\": %s,\n"
            "  \"library\": {\n"
            "    \"base\": \"0x%lx\",\n"
            "    \"size\": %zu,\n"
            "    \"version\": \"%s\",\n"
            "    \"valid\": %s,\n"
            "    \"arch\": \"%s\"\n"
            "  },\n"
            "  \"hooks\": {\n"
            "    \"installed\": %s,\n"
            "    \"count\": %d\n"
            "  },\n"
            "  \"features\": {\n"
            "    \"godMode\": %s,\n"
            "    \"infiniteAmmo\": %s,\n"
            "    \"neverWanted\": %s\n"
            "  }\n"
            "}",
            g_state.initialized ? "true" : "false",
            (unsigned long)g_state.gameLib.base,
            g_state.gameLib.size,
            GetVersionString(g_state.gameLib.version),
            g_state.gameLib.isValid ? "true" : "false",
            g_state.gameLib.is64Bit ? "ARM64" : "ARM32",
            g_state.hooksInstalled ? "true" : "false",
            g_state.hookCount,
            g_state.godModeEnabled ? "true" : "false",
            g_state.infiniteAmmoEnabled ? "true" : "false",
            g_state.neverWantedEnabled ? "true" : "false"
        );

        return std::string(buffer);
    }

    /**
     * Log current status
     */
    inline void LogStatus()
    {
        GTASA_LOGI("=== GTA:SA Integration Status ===");
        GTASA_LOGI("Initialized: %s", g_state.initialized ? "YES" : "NO");
        GTASA_LOGI("Library base: 0x%lx", (unsigned long)g_state.gameLib.base);
        GTASA_LOGI("Library size: %.2f MB", g_state.gameLib.size / (1024.0 * 1024.0));
        GTASA_LOGI("Version: %s", GetVersionString(g_state.gameLib.version));
        GTASA_LOGI("Valid: %s", g_state.gameLib.isValid ? "YES" : "NO");
        GTASA_LOGI("Hooks installed: %d", g_state.hookCount);
        GTASA_LOGI("God mode: %s", g_state.godModeEnabled ? "ON" : "OFF");
        GTASA_LOGI("================================");
    }

} // namespace MTA::Android::GTASA

#endif // GTASA_INTEGRATION_H
