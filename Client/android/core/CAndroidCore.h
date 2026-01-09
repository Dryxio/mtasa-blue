/*
 * MTA:SA Android - Core Module
 *
 * Central integration point that ties together all Android-specific modules:
 *   - Platform (input, filesystem, network)
 *   - Graphics (GLES rendering)
 *   - Game SA interface
 *   - Multiplayer hooks
 *
 * This is the main controller for the MTA Android client.
 */

#ifndef CANDROID_CORE_H
#define CANDROID_CORE_H

#include <cstdint>
#include <string>
#include <memory>
#include <atomic>
#include <mutex>

// Forward declarations
namespace MTA::Android::Platform
{
    class AndroidInput;
    class AndroidFileSystem;
    class AndroidNetwork;
}

namespace MTA::Android::Graphics
{
    class GLESGraphics;
}

namespace MTA::Android
{

//=============================================================================
// Client State
//=============================================================================

enum class ClientState
{
    Uninitialized,
    Initializing,
    Ready,
    Loading,
    InGame,
    Connecting,
    Connected,
    Error,
    ShuttingDown
};

//=============================================================================
// Connection Info
//=============================================================================

struct ServerInfo
{
    std::string host;
    uint16_t port;
    std::string serverName;
    std::string gameMode;
    std::string map;
    int players;
    int maxPlayers;
    bool passworded;
};

struct PlayerInfo
{
    std::string nick;
    uint32_t id;
    int ping;
    float health;
    float armor;
    bool inVehicle;
};

//=============================================================================
// CAndroidCore - Main Client Controller
//=============================================================================

class CAndroidCore
{
public:
    static CAndroidCore& Instance();

    //=========================================================================
    // Lifecycle
    //=========================================================================

    /**
     * Initialize the MTA client
     * @return true if initialization succeeded
     */
    bool Initialize();

    /**
     * Shutdown the client and release all resources
     */
    void Shutdown();

    /**
     * Check if client is initialized
     */
    bool IsInitialized() const { return m_state != ClientState::Uninitialized; }

    /**
     * Get current client state
     */
    ClientState GetState() const { return m_state; }

    //=========================================================================
    // Main Loop
    //=========================================================================

    /**
     * Process one frame
     * @param deltaTime Time since last frame in seconds
     */
    void Update(float deltaTime);

    /**
     * Render the current frame
     */
    void Render();

    /**
     * Called when the app is paused (backgrounded)
     */
    void OnPause();

    /**
     * Called when the app is resumed (foregrounded)
     */
    void OnResume();

    //=========================================================================
    // Display
    //=========================================================================

    /**
     * Called when the display surface changes
     * @param width New width in pixels
     * @param height New height in pixels
     */
    void OnSurfaceChanged(int width, int height);

    /**
     * Called when the display surface is destroyed
     */
    void OnSurfaceDestroyed();

    /**
     * Get current screen dimensions
     */
    void GetScreenSize(int& width, int& height) const;

    //=========================================================================
    // Server Connection
    //=========================================================================

    /**
     * Connect to a server
     * @param host Server hostname or IP
     * @param port Server port
     * @param nick Player nickname
     * @param password Server password (empty if none)
     * @return true if connection initiated
     */
    bool Connect(const std::string& host, uint16_t port,
                 const std::string& nick, const std::string& password = "");

    /**
     * Disconnect from the current server
     */
    void Disconnect();

    /**
     * Check if connected to a server
     */
    bool IsConnected() const;

    /**
     * Get connection state as integer (for JNI)
     * 0 = disconnected, 1 = connecting, 2 = connected
     */
    int GetConnectionStateInt() const;

    /**
     * Get current server info
     */
    const ServerInfo& GetServerInfo() const { return m_serverInfo; }

    /**
     * Get local player info
     */
    const PlayerInfo& GetLocalPlayer() const { return m_localPlayer; }

    //=========================================================================
    // Game Data
    //=========================================================================

    /**
     * Set path to GTA:SA game data
     */
    void SetGameDataPath(const std::string& path);

    /**
     * Get path to GTA:SA game data
     */
    std::string GetGameDataPath() const;

    /**
     * Check if game data is valid
     */
    bool ValidateGameData();

    //=========================================================================
    // Version Info
    //=========================================================================

    static const char* GetVersion() { return "1.6.0-android"; }
    static const char* GetBuildDate() { return __DATE__ " " __TIME__; }

    //=========================================================================
    // Module Access
    //=========================================================================

    Platform::AndroidInput& GetInput();
    Platform::AndroidFileSystem& GetFileSystem();
    Platform::AndroidNetwork& GetNetwork();
    Graphics::GLESGraphics* GetGraphics() { return m_graphics.get(); }

private:
    CAndroidCore();
    ~CAndroidCore();
    CAndroidCore(const CAndroidCore&) = delete;
    CAndroidCore& operator=(const CAndroidCore&) = delete;

    // State management
    void SetState(ClientState state);

    // Initialization phases
    bool InitializePlatform();
    bool InitializeGraphics();
    bool InitializeGame();
    bool InitializeMultiplayer();

    // Update phases
    void UpdateInput(float deltaTime);
    void UpdateNetwork(float deltaTime);
    void UpdateGame(float deltaTime);

    // Render phases
    void RenderGame();
    void RenderUI();

private:
    // State
    std::atomic<ClientState> m_state;
    bool m_paused;

    // Display
    int m_screenWidth;
    int m_screenHeight;

    // Server/Player info
    ServerInfo m_serverInfo;
    PlayerInfo m_localPlayer;

    // Modules
    std::unique_ptr<Graphics::GLESGraphics> m_graphics;

    // Thread safety
    std::mutex m_mutex;

    // Timing
    float m_totalTime;
    uint32_t m_frameCount;
    float m_fps;
    float m_fpsUpdateTime;
};

//=============================================================================
// Inline Implementations
//=============================================================================

inline CAndroidCore& CAndroidCore::Instance()
{
    static CAndroidCore instance;
    return instance;
}

inline bool CAndroidCore::IsConnected() const
{
    return m_state == ClientState::Connected || m_state == ClientState::InGame;
}

inline int CAndroidCore::GetConnectionStateInt() const
{
    switch (m_state.load())
    {
        case ClientState::Connecting:
            return 1;
        case ClientState::Connected:
        case ClientState::InGame:
            return 2;
        default:
            return 0;
    }
}

inline void CAndroidCore::GetScreenSize(int& width, int& height) const
{
    width = m_screenWidth;
    height = m_screenHeight;
}

} // namespace MTA::Android

#endif // CANDROID_CORE_H
