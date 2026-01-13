/*
 * MTA:SA Android - Server Connection
 *
 * Handles connection to MTA:SA servers using the MTA protocol.
 * This implements the connection handshake and maintains connection state.
 *
 * Phase 7: Multiplayer Logic
 */

#ifndef CSERVER_CONNECTION_ANDROID_H
#define CSERVER_CONNECTION_ANDROID_H

#include "CNetAndroid.h"
#include "CPacketHandler.h"
#include "raknet/RakNetHandshake.h"
#include <string>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <chrono>

namespace MTA::Android::Network
{

//=============================================================================
// Connection States
//=============================================================================

enum class ServerConnectionState
{
    DISCONNECTED,       // Not connected
    RESOLVING_DNS,      // Resolving server hostname
    CONNECTING,         // Creating socket
    RAKNET_HANDSHAKE,   // Performing RakNet handshake
    WAIT_MOD_NAME,      // Waiting for MOD_NAME packet
    SENDING_JOIN,       // Sending join data
    WAIT_JOIN_COMPLETE, // Waiting for join complete
    WAIT_JOINED_GAME,   // Waiting for joined game packet
    CONNECTED,          // Fully connected
    DISCONNECTING,      // Graceful disconnect in progress
    ERROR_STATE         // Connection error
};

//=============================================================================
// Connection Info
//=============================================================================

struct ServerInfo
{
    std::string host;
    uint16_t port = 22003;
    std::string serverName;
    std::string serverVersion;
    uint32_t playerCount = 0;
    uint32_t maxPlayers = 0;
};

struct PlayerInfo
{
    std::string nickname;
    std::string serial;
    std::string password;  // Will be MD5 hashed before sending
    uint8_t gameVersion = 0;  // GTA:SA version (0 = 1.0 US)
};

struct ConnectionResult
{
    bool success = false;
    std::string errorMessage;
    ServerConnectionState finalState = ServerConnectionState::DISCONNECTED;
    uint32_t playerId = 0;
    std::string serverName;
    std::string serverVersion;
};

//=============================================================================
// Connection Callbacks
//=============================================================================

struct ConnectionCallbacks
{
    std::function<void(ServerConnectionState state, const std::string& message)> onStateChanged;
    std::function<void(const ConnectionResult& result)> onConnected;
    std::function<void(const std::string& reason)> onDisconnected;
    std::function<void(const std::string& error)> onError;
};

//=============================================================================
// MTA Protocol Constants
//=============================================================================

// MTA version info (must match server expectations)
// These values must match the running MTA:SA server version
// For MTA 1.6.0 release servers:
// Formula: MTA_DM_NETCODE_VERSION = _NETCODE_VERSION + (_NETCODE_VERSION_BRANCH_ID << 12)
// Where _NETCODE_VERSION = 0x1DE and _NETCODE_VERSION_BRANCH_ID = 0x4 (trunk/release)
constexpr uint16_t MTA_DM_NETCODE_VERSION = 0x41DE;  // 0x1DE + (0x4 << 12) = release netcode
constexpr uint16_t MTA_DM_VERSION = 0x0160;          // 1.6.0 ((1<<8)|(6<<4)|0)
constexpr uint16_t MTA_DM_BITSTREAM_VERSION = 0x06B; // Bitstream version (0x06B = 107)

constexpr size_t MAX_PLAYER_NICK_LENGTH = 22;
constexpr size_t MAX_SERIAL_LENGTH = 32;
constexpr size_t MD5_HASH_LENGTH = 16;

// Packet IDs for connection (from MTA protocol)
constexpr uint8_t PACKET_ID_MOD_NAME = 28;
constexpr uint8_t PACKET_ID_PLAYER_JOINDATA = 1;
constexpr uint8_t PACKET_ID_SERVER_JOIN_COMPLETE = 2;
constexpr uint8_t PACKET_ID_SERVER_JOINEDGAME = 22;

//=============================================================================
// CServerConnection
//=============================================================================

class CServerConnection
{
public:
    CServerConnection();
    ~CServerConnection();

    /**
     * Initialize the connection system
     */
    bool Initialize();

    /**
     * Shutdown the connection system
     */
    void Shutdown();

    /**
     * Set connection callbacks
     */
    void SetCallbacks(const ConnectionCallbacks& callbacks);

    /**
     * Connect to an MTA server
     * @param server Server info (host, port)
     * @param player Player info (nickname, serial, password)
     * @return true if connection attempt started
     */
    bool Connect(const ServerInfo& server, const PlayerInfo& player);

    /**
     * Disconnect from server
     * @param reason Disconnect reason
     */
    void Disconnect(const std::string& reason = "");

    /**
     * Process connection (call in main loop)
     */
    void Process();

    /**
     * Get current connection state
     */
    ServerConnectionState GetState() const { return m_state; }

    /**
     * Get state as string
     */
    static const char* StateToString(ServerConnectionState state);

    /**
     * Check if connected
     */
    bool IsConnected() const { return m_state == ServerConnectionState::CONNECTED; }

    /**
     * Get server info
     */
    const ServerInfo& GetServerInfo() const { return m_serverInfo; }

    /**
     * Get assigned player ID
     */
    uint32_t GetPlayerId() const { return m_playerId; }

    /**
     * Send player position sync to server
     * Call this periodically while connected
     */
    void SendPlayerSync(float x, float y, float z, float rotation,
                        float vx, float vy, float vz, bool onGround);

    /**
     * Get network interface for direct access
     */
    CNetAndroid* GetNetwork() { return m_network; }

    //=========================================================================
    // Connection Test Methods (for development/debugging)
    //=========================================================================

    /**
     * Test basic UDP connectivity to server
     * @return true if server responds to ping
     */
    bool TestConnectivity(const std::string& host, uint16_t port, uint32_t timeoutMs = 5000);

    /**
     * Test DNS resolution
     * @return resolved IP address or empty string on failure
     */
    std::string TestDNSResolution(const std::string& hostname);

    /**
     * Get connection test results as JSON string
     */
    std::string GetConnectionTestResults() const;

private:
    //=========================================================================
    // Internal State Machine
    //=========================================================================

    void SetState(ServerConnectionState newState, const std::string& message = "");
    void ProcessState();

    // State handlers
    void ProcessResolvingDNS();
    void ProcessConnecting();
    void ProcessRakNetHandshake();
    void ProcessWaitModName();
    void ProcessSendingJoin();
    void ProcessWaitJoinComplete();
    void ProcessWaitJoinedGame();
    void ProcessConnected();

    //=========================================================================
    // Packet Handling
    //=========================================================================

    void HandleModNamePacket(NetBitStream& bitStream);
    void HandleJoinCompletePacket(NetBitStream& bitStream);
    void HandleJoinedGamePacket(NetBitStream& bitStream);

    void SendConnectionRequest();
    void SendJoinDataPacket();
    void SendDisconnectPacket();

    //=========================================================================
    // Utility Functions
    //=========================================================================

    void ComputeMD5Hash(const std::string& input, uint8_t output[MD5_HASH_LENGTH]);
    std::string GenerateSerial();
    uint64_t GetCurrentTimeMs();

private:
    // Connection state
    std::atomic<ServerConnectionState> m_state{ServerConnectionState::DISCONNECTED};
    ConnectionCallbacks m_callbacks;

    // Server info
    ServerInfo m_serverInfo;
    std::string m_resolvedIP;

    // Player info
    PlayerInfo m_playerInfo;
    uint32_t m_playerId = 0;

    // Network (use singleton reference, not owning pointer)
    CNetAndroid* m_network = nullptr;
    int m_socket = -1;

    // RakNet handshake
    std::unique_ptr<RakNet::RakNetHandshake> m_raknetHandshake;

    // Timing
    uint64_t m_stateStartTime = 0;
    uint64_t m_lastPacketTime = 0;
    uint32_t m_connectionTimeoutMs = 10000;
    uint32_t m_retryCount = 0;
    static constexpr uint32_t MAX_RETRIES = 3;

    // Connection test results
    struct TestResults
    {
        bool dnsResolved = false;
        std::string resolvedIP;
        uint32_t dnsTimeMs = 0;
        bool udpReachable = false;
        uint32_t pingMs = 0;
        bool serverResponded = false;
        std::string serverResponse;
        std::string lastError;
    } m_testResults;
};

//=============================================================================
// Simple MD5 Implementation (for password hashing)
//=============================================================================

class MD5
{
public:
    static void Compute(const uint8_t* data, size_t length, uint8_t output[16]);
    static void Compute(const std::string& str, uint8_t output[16]);
};

} // namespace MTA::Android::Network

#endif // CSERVER_CONNECTION_ANDROID_H
