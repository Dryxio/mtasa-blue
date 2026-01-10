/*
 * MTA:SA Android - Network Implementation
 *
 * CNetAndroid: Android implementation of CNet interface
 * Wraps RakNet for UDP-based client-server communication
 *
 * Phase 7: Multiplayer Logic
 */

#ifndef CNET_ANDROID_H
#define CNET_ANDROID_H

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <functional>
#include <thread>
#include <queue>

// Forward declarations
namespace RakNet { class BitStream; }

namespace MTA::Android::Network
{

//=============================================================================
// Protocol Constants
//=============================================================================

namespace Protocol
{
    constexpr uint16_t DEFAULT_PORT = 22003;
    constexpr uint16_t ASE_PORT_OFFSET = 123;
    constexpr uint16_t PROTOCOL_VERSION = 0x0166;  // MTA 1.6.x

    // Connection timeouts
    constexpr uint32_t CONNECT_TIMEOUT_MS = 10000;
    constexpr uint32_t DEFAULT_TIMEOUT_MS = 30000;

    // Packet priorities
    constexpr uint8_t PRIORITY_IMMEDIATE = 0;
    constexpr uint8_t PRIORITY_HIGH = 1;
    constexpr uint8_t PRIORITY_MEDIUM = 2;
    constexpr uint8_t PRIORITY_LOW = 3;

    // Reliability modes
    constexpr uint8_t RELIABILITY_UNRELIABLE = 0;
    constexpr uint8_t RELIABILITY_UNRELIABLE_SEQUENCED = 1;
    constexpr uint8_t RELIABILITY_RELIABLE = 2;
    constexpr uint8_t RELIABILITY_RELIABLE_ORDERED = 3;
    constexpr uint8_t RELIABILITY_RELIABLE_SEQUENCED = 4;
}

//=============================================================================
// Packet Types (from Shared/sdk/net/Packets.h)
//=============================================================================

enum class PacketID : uint8_t
{
    // Internal MTA packets
    SERVER_JOIN = 0,
    SERVER_JOIN_DATA,
    SERVER_JOIN_COMPLETE,

    PLAYER_JOIN,
    PLAYER_JOINDATA,
    PLAYER_QUIT,
    PLAYER_TIMEOUT,

    MOD_NAME,
    PACKET_PROGRESS,

    // Reserved
    MTA_RESERVED_03,
    MTA_RESERVED_04,
    MTA_RESERVED_05,
    MTA_RESERVED_06,
    MTA_RESERVED_07,
    MTA_RESERVED_08,
    MTA_RESERVED_09,
    MTA_RESERVED_10,
    MTA_RESERVED_11,
    MTA_RESERVED_12,
    MTA_RESERVED_13,
    MTA_RESERVED_14,
    MTA_RESERVED_15,

    // Connection packets
    SERVER_JOINEDGAME,
    SERVER_DISCONNECTED,

    // RPC
    RPC,

    // Player packets
    PLAYER_LIST,
    PLAYER_SPAWN,
    PLAYER_WASTED,
    PLAYER_CHANGE_NICK,
    PLAYER_STATS,
    PLAYER_CLOTHES,

    // Sync packets
    PLAYER_KEYSYNC,
    PLAYER_PURESYNC,
    PLAYER_VEHICLE_PURESYNC,
    LIGHTSYNC,
    VEHICLE_RESYNC,
    RETURN_SYNC,

    // Event packets
    EXPLOSION,
    FIRE,
    PROJECTILE,
    DETONATE_SATCHELS,
    DESTROY_SATCHELS,

    // Console/chat
    COMMAND,
    CHAT_ECHO,
    CONSOLE_ECHO,
    DEBUG_ECHO,

    // Map packets
    MAP_INFO,
    MAP_START,
    MAP_RESTART,
    MAP_STOP,

    // Entity packets
    ENTITY_ADD,
    ENTITY_REMOVE,
    PICKUP_HIDESHOW,
    PICKUP_HIT_CONFIRM,

    // Vehicle packets
    UNOCCUPIED_VEHICLE_STARTSYNC,
    UNOCCUPIED_VEHICLE_STOPSYNC,
    UNOCCUPIED_VEHICLE_SYNC,
    VEHICLE_SPAWN,
    VEHICLE_INOUT,
    VEHICLE_DAMAGE_SYNC,
    VEHICLE_TRAILER,

    // Ped sync
    PED_STARTSYNC,
    PED_STOPSYNC,
    PED_SYNC,
    PED_WASTED,

    // Voice
    VOICE_DATA,
    VOICE_END,

    // Lua
    LUA,
    LUA_ELEMENT_RPC,
    LUA_EVENT,

    // Resources
    RESOURCE_START,
    RESOURCE_STOP,

    // Camera
    CAMERA_SYNC,

    // Object sync
    OBJECT_STARTSYNC,
    OBJECT_STOPSYNC,
    OBJECT_SYNC,

    // Misc
    UPDATE_INFO,
    DISCONNECT_MESSAGE,
    PLAYER_TRANSGRESSION,
    PLAYER_DIAGNOSTIC,
    PLAYER_MODINFO,
    PLAYER_SCREENSHOT,
    RESOURCE_CLIENT_SCRIPTS,
    LATENT_TRANSFER,
    VEHICLE_PUSH_SYNC,
    PLAYER_BULLETSYNC,
    SYNC_SETTINGS,
    WEAPON_BULLETSYNC,
    PED_TASK,
    PLAYER_NO_SOCKET,
    PLAYER_NETWORK_STATUS,
    PLAYER_ACINFO,
    CHAT_CLEAR,
    SERVER_INFO_SYNC,
    DISCORD_JOIN,
    PLAYER_RESOURCE_START,
    PLAYER_WORLD_SPECIAL_PROPERTY
};

//=============================================================================
// Connection State
//=============================================================================

enum class ConnectionState
{
    Disconnected,
    Connecting,
    Connected,
    Disconnecting,
    Failed
};

//=============================================================================
// Network Statistics
//=============================================================================

struct NetStatistics
{
    // Bytes
    uint64_t bytesSent = 0;
    uint64_t bytesReceived = 0;
    uint64_t bytesResent = 0;

    // Packets
    uint64_t packetsSent = 0;
    uint64_t packetsReceived = 0;
    uint64_t packetsLost = 0;

    // Timing
    uint32_t ping = 0;
    uint32_t pingVariance = 0;
    uint64_t connectionStartTime = 0;

    // Bandwidth
    float sendBandwidth = 0.0f;
    float receiveBandwidth = 0.0f;

    // Loss rate
    float packetLossRate = 0.0f;
};

//=============================================================================
// Packet Statistics
//=============================================================================

struct PacketStat
{
    int count = 0;
    int totalBytes = 0;
    uint64_t totalTime = 0;
};

//=============================================================================
// Network Bitstream
//=============================================================================

class NetBitStream
{
public:
    NetBitStream();
    NetBitStream(const uint8_t* data, size_t sizeInBytes);
    ~NetBitStream();

    // Reset
    void Reset();
    void ResetReadPointer();

    // Write operations
    void Write(uint8_t value);
    void Write(int8_t value);
    void Write(uint16_t value);
    void Write(int16_t value);
    void Write(uint32_t value);
    void Write(int32_t value);
    void Write(float value);
    void Write(double value);
    void Write(const char* data, size_t length);
    void Write(const std::string& str);

    void WriteCompressed(uint16_t value);
    void WriteCompressed(int16_t value);
    void WriteCompressed(uint32_t value);
    void WriteCompressed(int32_t value);

    void WriteBits(const uint8_t* data, size_t numBits);
    void WriteBit(bool value);

    // Compressed vectors
    void WriteNormVector(float x, float y, float z);
    void WriteVector(float x, float y, float z);
    void WriteNormQuat(float w, float x, float y, float z);

    // Read operations
    bool Read(uint8_t& value);
    bool Read(int8_t& value);
    bool Read(uint16_t& value);
    bool Read(int16_t& value);
    bool Read(uint32_t& value);
    bool Read(int32_t& value);
    bool Read(float& value);
    bool Read(double& value);
    bool Read(char* data, size_t length);
    bool Read(std::string& str, size_t maxLength);

    bool ReadCompressed(uint16_t& value);
    bool ReadCompressed(int16_t& value);
    bool ReadCompressed(uint32_t& value);
    bool ReadCompressed(int32_t& value);

    bool ReadBits(uint8_t* data, size_t numBits);
    bool ReadBit();

    // Compressed vectors
    bool ReadNormVector(float& x, float& y, float& z);
    bool ReadVector(float& x, float& y, float& z);
    bool ReadNormQuat(float& w, float& x, float& y, float& z);

    // Utilities
    int GetReadOffsetBits() const;
    void SetReadOffsetBits(int offset);
    int GetWriteOffsetBits() const;

    int GetBitsUsed() const;
    int GetBytesUsed() const;
    int GetUnreadBits() const;

    void AlignWriteToByte();
    void AlignReadToByte();

    const uint8_t* GetData() const;
    uint8_t* GetData();

    bool CanReadBytes(size_t numBytes) const;

    // Version
    uint16_t GetVersion() const { return m_version; }
    void SetVersion(uint16_t version) { m_version = version; }

private:
    void EnsureCapacity(size_t bitsNeeded);

    std::vector<uint8_t> m_data;
    size_t m_readOffsetBits = 0;
    size_t m_writeOffsetBits = 0;
    uint16_t m_version = Protocol::PROTOCOL_VERSION;
};

//=============================================================================
// Packet Structure
//=============================================================================

struct Packet
{
    PacketID id;
    std::vector<uint8_t> data;
    uint32_t timestamp;
    std::string senderAddress;
    uint16_t senderPort;
};

//=============================================================================
// Packet Handler Callback
//=============================================================================

using PacketHandler = std::function<bool(PacketID id, NetBitStream& bitStream)>;

//=============================================================================
// CNetAndroid - Main Network Class
//=============================================================================

class CNetAndroid
{
public:
    static CNetAndroid& Instance();

    //=========================================================================
    // Initialization
    //=========================================================================

    bool Initialize();
    void Shutdown();

    //=========================================================================
    // Connection
    //=========================================================================

    /**
     * Start connection to server
     * @param host Server hostname or IP
     * @param port Server port
     * @return true if connection initiated
     */
    bool Connect(const std::string& host, uint16_t port);

    /**
     * Disconnect from server
     * @param reason Disconnect reason string
     */
    void Disconnect(const std::string& reason = "");

    /**
     * Check if connected
     */
    bool IsConnected() const { return m_state == ConnectionState::Connected; }

    /**
     * Check if ready to send/receive
     */
    bool IsReady() const { return m_state == ConnectionState::Connected; }

    /**
     * Get connection state
     */
    ConnectionState GetState() const { return m_state; }

    /**
     * Get connected server address
     */
    std::string GetConnectedServer(bool includePort = false) const;

    //=========================================================================
    // Packet Operations
    //=========================================================================

    /**
     * Allocate a new bitstream for sending
     */
    std::unique_ptr<NetBitStream> AllocateBitStream();

    /**
     * Send a packet to the server
     * @param packetId Packet type
     * @param bitStream Data to send
     * @param priority Packet priority (0-3)
     * @param reliability Reliability mode
     * @param orderingChannel Ordering channel for ordered packets
     * @return true if packet was queued
     */
    bool SendPacket(PacketID packetId, NetBitStream& bitStream,
                    uint8_t priority = Protocol::PRIORITY_MEDIUM,
                    uint8_t reliability = Protocol::RELIABILITY_RELIABLE,
                    uint8_t orderingChannel = 0);

    /**
     * Register packet handler
     */
    void RegisterPacketHandler(PacketHandler handler);

    /**
     * Process network updates (call every frame)
     */
    void DoPulse();

    //=========================================================================
    // Statistics
    //=========================================================================

    /**
     * Get ping to server
     */
    int GetPing() const { return m_ping; }

    /**
     * Get network time
     */
    uint32_t GetTime() const;

    /**
     * Get network statistics
     */
    const NetStatistics& GetStatistics() const { return m_stats; }

    /**
     * Get packet statistics
     */
    const PacketStat* GetPacketStats() const { return m_packetStats; }

    //=========================================================================
    // Configuration
    //=========================================================================

    /**
     * Set client port
     */
    void SetClientPort(uint16_t port) { m_clientPort = port; }

    /**
     * Set timeout time in ms
     */
    void SetTimeoutTime(uint32_t timeoutMs) { m_timeoutMs = timeoutMs; }

    /**
     * Get local IP address
     */
    std::string GetLocalIP() const;

    /**
     * Get device serial (for server identification)
     */
    std::string GetSerial() const;

    /**
     * Set bitstream version
     */
    void SetServerBitStreamVersion(uint16_t version) { m_serverBitStreamVersion = version; }
    uint16_t GetServerBitStreamVersion() const { return m_serverBitStreamVersion; }

    //=========================================================================
    // Error Handling
    //=========================================================================

    /**
     * Get connection error code
     */
    uint8_t GetConnectionError() const { return m_connectionError; }

    /**
     * Get connection error string
     */
    std::string GetConnectionErrorString() const;

    /**
     * Reset connection state
     */
    void Reset();

private:
    CNetAndroid();
    ~CNetAndroid();
    CNetAndroid(const CNetAndroid&) = delete;
    CNetAndroid& operator=(const CNetAndroid&) = delete;

    // Internal methods
    void NetworkThread();
    void ProcessReceivedPackets();
    void UpdateStatistics();
    bool ResolveHost(const std::string& host, std::string& outIP);

private:
    // State
    std::atomic<ConnectionState> m_state{ConnectionState::Disconnected};
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_running{false};

    // Connection info
    std::string m_serverHost;
    std::string m_serverIP;
    uint16_t m_serverPort = 0;
    uint16_t m_clientPort = 0;

    // Timing
    std::atomic<int> m_ping{0};
    uint32_t m_timeoutMs = Protocol::DEFAULT_TIMEOUT_MS;
    uint64_t m_connectionStartTime = 0;
    uint64_t m_lastPacketTime = 0;

    // Error state
    std::atomic<uint8_t> m_connectionError{0};

    // Protocol version
    uint16_t m_serverBitStreamVersion = Protocol::PROTOCOL_VERSION;

    // Statistics
    NetStatistics m_stats;
    PacketStat m_packetStats[256];

    // Packet handler
    PacketHandler m_packetHandler;

    // Packet queues
    std::queue<Packet> m_incomingPackets;
    std::queue<std::pair<PacketID, std::vector<uint8_t>>> m_outgoingPackets;
    mutable std::mutex m_incomingMutex;
    mutable std::mutex m_outgoingMutex;

    // Network thread
    std::thread m_networkThread;

    // Socket handle (platform-specific)
    int m_socket = -1;
};

//=============================================================================
// Inline Implementations
//=============================================================================

inline CNetAndroid& CNetAndroid::Instance()
{
    static CNetAndroid instance;
    return instance;
}

} // namespace MTA::Android::Network

#endif // CNET_ANDROID_H
