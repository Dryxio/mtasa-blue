/**
 * MTA:SA Android - RakNet Handshake
 *
 * Implements the RakNet connection handshake protocol for MTA servers.
 *
 * MTA uses RakNet 3.x protocol (not RakNet 4).
 *
 * RakNet 3.x Connection Sequence:
 * 1. Client -> Server: OPEN_CONNECTION_REQUEST (ID=9) + cookie
 * 2. Server -> Client: OPEN_CONNECTION_REPLY (ID=10) + cookie
 * 3. Client -> Server: CONNECTION_REQUEST (ID=4) + GUID + timestamp
 * 4. Server -> Client: CONNECTION_REQUEST_ACCEPTED (ID=14) + ...
 * 5. After this, MTA protocol takes over (MOD_NAME, etc.)
 *
 * We also support RakNet 4 for testing with custom servers.
 */

#ifndef RAKNET_HANDSHAKE_H
#define RAKNET_HANDSHAKE_H

#include <cstdint>
#include <cstring>
#include <string>
#include <functional>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#include <errno.h>

#include <android/log.h>
#define LOG_TAG_RAKNET "MTA-RakNet"
#define RAKNET_LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG_RAKNET, __VA_ARGS__)
#define RAKNET_LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG_RAKNET, __VA_ARGS__)
#define RAKNET_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG_RAKNET, __VA_ARGS__)

namespace RakNet
{

//=============================================================================
// RakNet Protocol Constants
//=============================================================================

// RakNet magic bytes (offline message identifier)
static const uint8_t RAKNET_MAGIC[16] = {
    0x00, 0xff, 0xff, 0x00, 0xfe, 0xfe, 0xfe, 0xfe,
    0xfd, 0xfd, 0xfd, 0xfd, 0x12, 0x34, 0x56, 0x78
};

// RakNet protocol version (MTA uses RakNet 4)
static const uint8_t RAKNET_PROTOCOL_VERSION = 6;

// Default MTU sizes
static const uint16_t RAKNET_MTU_MAX = 1492;
static const uint16_t RAKNET_MTU_MIN = 400;

// MTA RakNet 3.x Packet IDs (from packetenums.h)
enum MTARakNetPacketID : uint8_t
{
    // RakNet 3.x internal IDs (used by MTA)
    MTA_RID_INTERNAL_PING = 0x00,
    MTA_RID_PING = 0x01,
    MTA_RID_PING_OPEN_CONNECTIONS = 0x02,
    MTA_RID_CONNECTED_PONG = 0x03,
    MTA_RID_CONNECTION_REQUEST = 0x04,          // Client -> Server: connection request
    MTA_RID_SECURED_CONNECTION_RESPONSE = 0x05,
    MTA_RID_SECURED_CONNECTION_CONFIRMATION = 0x06,
    MTA_RID_RPC_MAPPING = 0x07,
    MTA_RID_DETECT_LOST_CONNECTIONS = 0x08,
    MTA_RID_OPEN_CONNECTION_REQUEST = 0x09,     // Client -> Server: initial contact
    MTA_RID_OPEN_CONNECTION_REPLY = 0x0A,       // Server -> Client: reply to open
    MTA_RID_RPC = 0x0B,
    MTA_RID_RPC_REPLY = 0x0C,
    MTA_RID_OUT_OF_BAND_INTERNAL = 0x0D,
    // User types start at 0x0E
    MTA_RID_CONNECTION_REQUEST_ACCEPTED = 0x0E, // Server -> Client: accepted
    MTA_RID_CONNECTION_ATTEMPT_FAILED = 0x0F,
    MTA_RID_ALREADY_CONNECTED = 0x10,
    MTA_RID_NEW_INCOMING_CONNECTION = 0x11,
    MTA_RID_NO_FREE_INCOMING_CONNECTIONS = 0x12,
    MTA_RID_DISCONNECTION_NOTIFICATION = 0x13,
    MTA_RID_CONNECTION_LOST = 0x14,
    MTA_RID_RSA_PUBLIC_KEY_MISMATCH = 0x15,
    MTA_RID_CONNECTION_BANNED = 0x16,
    MTA_RID_INVALID_PASSWORD = 0x17,
    MTA_RID_INCOMPATIBLE_PROTOCOL_VERSION = 0x44, // 68
};

// RakNet 4 Packet IDs (for testing with custom servers)
enum RakNet4PacketID : uint8_t
{
    ID_CONNECTED_PING = 0x00,
    ID_UNCONNECTED_PING = 0x01,
    ID_UNCONNECTED_PING_OPEN = 0x02,
    ID_CONNECTED_PONG = 0x03,
    ID_DETECT_LOST_CONNECTIONS = 0x04,
    ID_OPEN_CONNECTION_REQUEST_1 = 0x05,
    ID_OPEN_CONNECTION_REPLY_1 = 0x06,
    ID_OPEN_CONNECTION_REQUEST_2 = 0x07,
    ID_OPEN_CONNECTION_REPLY_2 = 0x08,
    ID_CONNECTION_REQUEST = 0x09,
    ID_REMOTE_SYSTEM_REQUIRES_PUBLIC_KEY = 0x0A,
    ID_OUR_SYSTEM_REQUIRES_SECURITY = 0x0B,
    ID_PUBLIC_KEY_MISMATCH = 0x0C,
    ID_OUT_OF_BAND_INTERNAL = 0x0D,
    ID_SND_RECEIPT_ACKED = 0x0E,
    ID_SND_RECEIPT_LOSS = 0x0F,
    ID_CONNECTION_REQUEST_ACCEPTED = 0x10,
    ID_CONNECTION_ATTEMPT_FAILED = 0x11,
    ID_ALREADY_CONNECTED = 0x12,
    ID_NEW_INCOMING_CONNECTION = 0x13,
    ID_NO_FREE_INCOMING_CONNECTIONS = 0x14,
    ID_DISCONNECTION_NOTIFICATION = 0x15,
    ID_CONNECTION_LOST = 0x16,
    ID_CONNECTION_BANNED = 0x17,
    ID_INVALID_PASSWORD = 0x18,
    ID_INCOMPATIBLE_PROTOCOL_VERSION = 0x19,
    ID_IP_RECENTLY_CONNECTED = 0x1A,
    ID_TIMESTAMP = 0x1B,
    ID_UNCONNECTED_PONG = 0x1C,
    ID_ADVERTISE_SYSTEM = 0x1D,
    ID_DOWNLOAD_PROGRESS = 0x1E,
    ID_USER_PACKET_ENUM = 0x80
};

// Protocol mode
enum class RakNetProtocol
{
    MTA_RAKNET3,    // MTA's RakNet 3.x protocol (default for real servers)
    RAKNET4         // Standard RakNet 4 (for testing)
};

//=============================================================================
// RakNet GUID
//=============================================================================

struct RakNetGUID
{
    uint64_t g;

    RakNetGUID() : g(0) {}
    RakNetGUID(uint64_t val) : g(val) {}

    static RakNetGUID Generate()
    {
        RakNetGUID guid;
        // Simple random GUID generation
        guid.g = ((uint64_t)rand() << 32) | rand();
        return guid;
    }
};

//=============================================================================
// Handshake State
//=============================================================================

enum class HandshakeState
{
    DISCONNECTED,
    SENDING_REQUEST_1,
    WAITING_REPLY_1,
    SENDING_REQUEST_2,
    WAITING_REPLY_2,
    SENDING_CONNECTION_REQUEST,
    WAITING_CONNECTION_ACCEPTED,
    CONNECTED,
    FAILED
};

const char* HandshakeStateToString(HandshakeState state);

//=============================================================================
// RakNet Handshake Client
//=============================================================================

class RakNetHandshake
{
public:
    RakNetHandshake();
    ~RakNetHandshake();

    /**
     * Start handshake with server
     * @param socket Already connected UDP socket
     * @param serverAddr Server address
     * @param protocol Protocol to use (MTA_RAKNET3 for real servers, RAKNET4 for test)
     * @return true if handshake started
     */
    bool Start(int socket, const sockaddr_in& serverAddr,
               RakNetProtocol protocol = RakNetProtocol::MTA_RAKNET3);

    /**
     * Process handshake (call repeatedly until complete or failed)
     * @return Current state
     */
    HandshakeState Process();

    /**
     * Get current state
     */
    HandshakeState GetState() const { return m_state; }

    /**
     * Check if handshake completed successfully
     */
    bool IsConnected() const { return m_state == HandshakeState::CONNECTED; }

    /**
     * Check if handshake failed
     */
    bool HasFailed() const { return m_state == HandshakeState::FAILED; }

    /**
     * Get server GUID (valid after connection)
     */
    RakNetGUID GetServerGUID() const { return m_serverGUID; }

    /**
     * Get negotiated MTU
     */
    uint16_t GetMTU() const { return m_mtu; }

    /**
     * Get error message (if failed)
     */
    const std::string& GetError() const { return m_error; }

private:
    // RakNet 4 Packet building (for test servers)
    int BuildOpenConnectionRequest1_RN4(uint8_t* buffer);
    int BuildOpenConnectionRequest2_RN4(uint8_t* buffer);
    int BuildConnectionRequest_RN4(uint8_t* buffer);

    // MTA RakNet 3.x Packet building (for real MTA servers)
    int BuildOpenConnectionRequest_MTA(uint8_t* buffer);
    int BuildConnectionRequest_MTA(uint8_t* buffer);

    // RakNet 4 Packet handling
    bool HandleOpenConnectionReply1_RN4(const uint8_t* data, int length);
    bool HandleOpenConnectionReply2_RN4(const uint8_t* data, int length);
    bool HandleConnectionRequestAccepted_RN4(const uint8_t* data, int length);

    // MTA RakNet 3.x Packet handling
    bool HandleOpenConnectionReply_MTA(const uint8_t* data, int length);
    bool HandleConnectionRequestAccepted_MTA(const uint8_t* data, int length);

    // Common
    bool HandleConnectionFailed(const uint8_t* data, int length, const char* reason);

    // Send/receive
    bool SendPacket(const uint8_t* data, int length);
    int ReceivePacket(uint8_t* buffer, int maxLength, int timeoutMs);

    // Utility
    void SetState(HandshakeState newState);
    uint64_t GetTimeMs();
    void WriteAddress(uint8_t* buffer, const sockaddr_in& addr);

private:
    HandshakeState m_state = HandshakeState::DISCONNECTED;
    RakNetProtocol m_protocol = RakNetProtocol::MTA_RAKNET3;
    std::string m_error;

    int m_socket = -1;
    sockaddr_in m_serverAddr;

    RakNetGUID m_clientGUID;
    RakNetGUID m_serverGUID;
    uint16_t m_mtu = RAKNET_MTU_MAX;
    uint32_t m_cookie = 0;  // MTA RakNet 3.x cookie

    uint64_t m_stateStartTime = 0;
    uint32_t m_retryCount = 0;
    static const uint32_t MAX_RETRIES = 5;
    static const uint32_t RETRY_TIMEOUT_MS = 1000;
};

} // namespace RakNet

#endif // RAKNET_HANDSHAKE_H
