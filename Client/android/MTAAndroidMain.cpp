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
#include "game_sa/CPlayerSync.h"
#include "game_sa/CGameBypass.h"
#include "network/CServerConnection.h"
#include <thread>
#include <memory>
#include <atomic>

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

    // Server connection
    static std::unique_ptr<Network::CServerConnection> g_serverConnection;
    static std::thread g_connectionThread;
    static std::atomic<bool> g_connectionRunning{false};

    // Player sync
    static std::unique_ptr<Sync::CPlayerSync> g_playerSync;

    // Game bypass (auto-spawn)
    static bool g_gameBypassInitialized = false;

    // Forward declarations
    void StartPlayerSync();
    void InitializeGameBypass();

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

    // =============================================================================
    // Server Connection
    // =============================================================================

    /**
     * Connection thread function - runs in background
     */
    void ConnectionThreadFunc()
    {
        LOGI("Connection thread started");

        if (!g_serverConnection)
        {
            LOGE("Server connection not created");
            return;
        }

        // Process connection until done or stopped
        while (g_connectionRunning.load())
        {
            g_serverConnection->Process();

            // Check connection state
            auto state = g_serverConnection->GetState();
            if (state == Network::ServerConnectionState::CONNECTED ||
                state == Network::ServerConnectionState::ERROR_STATE ||
                state == Network::ServerConnectionState::DISCONNECTED)
            {
                // Connection complete or failed
                break;
            }

            // Small delay to avoid busy loop
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        LOGI("Connection thread finished, state: %d", (int)g_serverConnection->GetState());
    }

    /**
     * Connect to MTA server
     * Called after game initialization
     */
    bool ConnectToServer()
    {
        LOGI("===========================================");
        LOGI("MTA:SA Android - Connecting to Server");
        LOGI("===========================================");

        // Create server connection if needed
        if (!g_serverConnection)
        {
            g_serverConnection = std::make_unique<Network::CServerConnection>();
        }

        // Initialize
        if (!g_serverConnection->Initialize())
        {
            LOGE("Failed to initialize server connection");
            return false;
        }

        // Set up callbacks
        Network::ConnectionCallbacks callbacks;
        callbacks.onStateChanged = [](Network::ServerConnectionState state, const std::string& message) {
            LOGI("Connection state: %d - %s", (int)state, message.c_str());
        };
        callbacks.onConnected = [](const Network::ConnectionResult& result) {
            LOGI("===========================================");
            LOGI("CONNECTED TO MTA SERVER!");
            LOGI("Player ID: %u", result.playerId);
            LOGI("Server: %s", result.serverName.c_str());
            LOGI("Version: %s", result.serverVersion.c_str());
            LOGI("===========================================");

            // Start player sync after successful connection
            StartPlayerSync();
        };
        callbacks.onDisconnected = [](const std::string& reason) {
            LOGI("Disconnected: %s", reason.c_str());
        };
        callbacks.onError = [](const std::string& error) {
            LOGE("Connection error: %s", error.c_str());
        };
        g_serverConnection->SetCallbacks(callbacks);

        // Server info - VPS MTA server with net_android.so
        Network::ServerInfo server;
        server.host = "37.59.101.35";
        server.port = 22004;

        // Player info
        Network::PlayerInfo player;
        player.nickname = "AndroidPlayer";
        player.serial = "ANDROID000000000000000000000001";
        player.password = "";  // No password

        LOGI("Connecting to %s:%d as '%s'...", server.host.c_str(), server.port, player.nickname.c_str());

        // Start connection
        if (!g_serverConnection->Connect(server, player))
        {
            LOGE("Failed to start connection");
            return false;
        }

        // Start connection thread
        g_connectionRunning = true;
        g_connectionThread = std::thread(ConnectionThreadFunc);

        return true;
    }

    /**
     * Initialize game bypass system for auto-spawn
     */
    void InitializeGameBypass()
    {
        LOGI(">>> InitializeGameBypass() called");

        if (g_gameBypassInitialized)
        {
            LOGW("Game bypass already initialized");
            return;
        }

        LOGI(">>> g_gameBypassInitialized is false, continuing...");

        if (!g_gtasaLib.loaded)
        {
            LOGE("Cannot initialize game bypass: GTA:SA not loaded");
            return;
        }

        LOGI(">>> g_gtasaLib.loaded is true, initializing game bypass...");

        auto& gameBypass = Game::CGameBypass::GetInstance();

        if (!gameBypass.Initialize(g_gtasaLib.base))
        {
            LOGE("Failed to initialize game bypass");
            return;
        }

        // Set callback for when player spawns
        gameBypass.SetOnSpawnCallback([]() {
            LOGI("Player spawned! Starting position sync...");
            StartPlayerSync();
        });

        g_gameBypassInitialized = true;
        LOGI("Game bypass system initialized");

        // Start game bypass processing thread
        std::thread([]() {
            LOGI("Game bypass processing thread started");
            auto& bypass = Game::CGameBypass::GetInstance();

            while (g_initialized)
            {
                // Process game bypass (checks for game ready and triggers spawn)
                bypass.Process();

                // Log game state periodically
                static int logCounter = 0;
                if (++logCounter >= 50)  // Every 5 seconds
                {
                    auto state = bypass.GetGameState();
                    LOGI("Game state: %d, Ready: %s, Spawned: %s",
                         (int)state,
                         bypass.IsGameReadyForSpawn() ? "yes" : "no",
                         bypass.IsLocalPlayerSpawned() ? "yes" : "no");
                    logCounter = 0;
                }

                // Check every 100ms
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            LOGI("Game bypass processing thread stopped");
        }).detach();
    }

    /**
     * Start player position sync after connection
     */
    void StartPlayerSync()
    {
        LOGI("Starting player sync...");

        if (!g_gtasaLib.loaded)
        {
            LOGE("Cannot start sync: GTA:SA not loaded");
            return;
        }

        // Create player sync if needed
        if (!g_playerSync)
        {
            g_playerSync = std::make_unique<Sync::CPlayerSync>();
        }

        // Initialize with game base
        if (!g_playerSync->Initialize(g_gtasaLib.base))
        {
            LOGE("Failed to initialize player sync");
            return;
        }

        // Set callback to send position to server
        g_playerSync->SetPositionCallback([](const Sync::PlayerSyncData& data) {
            // For now, just log (position reading test)
            // Later: send to server via g_serverConnection->SendPositionSync(data)
            static int logCounter = 0;
            if (++logCounter >= 50)  // Log every 5 seconds
            {
                LOGI("SYNC: pos=(%.1f, %.1f, %.1f) health=%d vehicle=%s",
                     data.position.x, data.position.y, data.position.z,
                     data.health, data.isInVehicle ? "yes" : "no");
                logCounter = 0;
            }
        });

        // Start sync loop
        if (g_playerSync->Start())
        {
            LOGI("Player sync started successfully!");
        }
        else
        {
            LOGE("Failed to start player sync");
        }
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

        // Initialize GTA:SA integration (without hooks for now)
        if (GTASA::Initialize())
        {
            LOGI("GTA:SA integration initialized!");
        }
        else
        {
            LOGW("GTA:SA integration failed - continuing without it");
        }

        g_initialized = true;
        LOGI("MTA:SA Android initialized successfully!");

        // Initialize game bypass for auto-spawn
        // This will monitor game state and spawn player when ready
        InitializeGameBypass();

        // Connect to MTA server in background
        // Small delay to let the game initialize first
        std::thread([]() {
            std::this_thread::sleep_for(std::chrono::seconds(3));
            ConnectToServer();
        }).detach();

        return true;
    }

    /**
     * Cleanup function
     */
    void Shutdown()
    {
        if (!g_initialized) return;

        LOGI("MTA:SA Android shutting down...");

        // Stop player sync
        if (g_playerSync)
        {
            g_playerSync->Stop();
            g_playerSync.reset();
        }

        // Stop connection thread
        g_connectionRunning = false;
        if (g_connectionThread.joinable())
        {
            g_connectionThread.join();
        }

        // Disconnect from server
        if (g_serverConnection)
        {
            g_serverConnection->Disconnect("Shutdown");
            g_serverConnection.reset();
        }

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
