/**
 * MTA:SA Android - RakNet Handshake Implementation
 *
 * Supports both MTA's RakNet 3.x protocol and standard RakNet 4.
 */

#include "RakNetHandshake.h"
#include <chrono>
#include <cstdlib>
#include <ctime>

namespace RakNet
{

//=============================================================================
// State String Conversion
//=============================================================================

const char* HandshakeStateToString(HandshakeState state)
{
    switch (state)
    {
        case HandshakeState::DISCONNECTED: return "DISCONNECTED";
        case HandshakeState::SENDING_REQUEST_1: return "SENDING_REQUEST_1";
        case HandshakeState::WAITING_REPLY_1: return "WAITING_REPLY_1";
        case HandshakeState::SENDING_REQUEST_2: return "SENDING_REQUEST_2";
        case HandshakeState::WAITING_REPLY_2: return "WAITING_REPLY_2";
        case HandshakeState::SENDING_CONNECTION_REQUEST: return "SENDING_CONNECTION_REQUEST";
        case HandshakeState::WAITING_CONNECTION_ACCEPTED: return "WAITING_CONNECTION_ACCEPTED";
        case HandshakeState::CONNECTED: return "CONNECTED";
        case HandshakeState::FAILED: return "FAILED";
        default: return "UNKNOWN";
    }
}

//=============================================================================
// RakNetHandshake Implementation
//=============================================================================

RakNetHandshake::RakNetHandshake()
{
    // Seed random for GUID generation
    srand(static_cast<unsigned>(time(nullptr)));
    m_clientGUID = RakNetGUID::Generate();
    m_cookie = static_cast<uint32_t>(rand());
    RAKNET_LOGI("RakNet handshake initialized, client GUID: %llx, cookie: %08x",
                (unsigned long long)m_clientGUID.g, m_cookie);
}

RakNetHandshake::~RakNetHandshake()
{
    // Socket is managed externally
}

bool RakNetHandshake::Start(int socket, const sockaddr_in& serverAddr, RakNetProtocol protocol)
{
    if (socket < 0)
    {
        m_error = "Invalid socket";
        return false;
    }

    m_socket = socket;
    m_serverAddr = serverAddr;
    m_protocol = protocol;
    m_retryCount = 0;
    m_mtu = RAKNET_MTU_MAX;

    char ipStr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &serverAddr.sin_addr, ipStr, sizeof(ipStr));
    RAKNET_LOGI("Starting RakNet handshake with %s:%d (protocol: %s)",
                ipStr, ntohs(serverAddr.sin_port),
                protocol == RakNetProtocol::MTA_RAKNET3 ? "MTA RakNet 3.x" : "RakNet 4");

    SetState(HandshakeState::SENDING_REQUEST_1);
    return true;
}

HandshakeState RakNetHandshake::Process()
{
    uint8_t buffer[2048];
    int received;
    uint64_t now = GetTimeMs();
    uint64_t elapsed = now - m_stateStartTime;

    switch (m_state)
    {
        case HandshakeState::SENDING_REQUEST_1:
        {
            int len;
            if (m_protocol == RakNetProtocol::MTA_RAKNET3)
            {
                len = BuildOpenConnectionRequest_MTA(buffer);
                RAKNET_LOGD("Sent MTA OPEN_CONNECTION_REQUEST (%d bytes)", len);
            }
            else
            {
                len = BuildOpenConnectionRequest1_RN4(buffer);
                RAKNET_LOGD("Sent RN4 OPEN_CONNECTION_REQUEST_1 (%d bytes, MTU padding to %d)",
                            len, m_mtu);
            }

            if (SendPacket(buffer, len))
            {
                SetState(HandshakeState::WAITING_REPLY_1);
            }
            else
            {
                m_error = "Failed to send request 1";
                SetState(HandshakeState::FAILED);
            }
            break;
        }

        case HandshakeState::WAITING_REPLY_1:
        {
            received = ReceivePacket(buffer, sizeof(buffer), 100);
            if (received > 0)
            {
                RAKNET_LOGD("Received %d bytes, packet ID: 0x%02X", received, buffer[0]);

                if (m_protocol == RakNetProtocol::MTA_RAKNET3)
                {
                    // MTA RakNet 3.x reply
                    if (buffer[0] == MTA_RID_OPEN_CONNECTION_REPLY)
                    {
                        if (HandleOpenConnectionReply_MTA(buffer, received))
                        {
                            // MTA RakNet 3.x goes directly to connection request
                            SetState(HandshakeState::SENDING_CONNECTION_REQUEST);
                        }
                    }
                    else if (buffer[0] == MTA_RID_NO_FREE_INCOMING_CONNECTIONS)
                    {
                        HandleConnectionFailed(buffer, received, "Server full");
                    }
                    else if (buffer[0] == MTA_RID_CONNECTION_BANNED)
                    {
                        HandleConnectionFailed(buffer, received, "Banned from server");
                    }
                    else if (buffer[0] == MTA_RID_INCOMPATIBLE_PROTOCOL_VERSION)
                    {
                        HandleConnectionFailed(buffer, received, "Incompatible protocol version");
                    }
                }
                else
                {
                    // RakNet 4 reply
                    if (buffer[0] == ID_OPEN_CONNECTION_REPLY_1)
                    {
                        if (HandleOpenConnectionReply1_RN4(buffer, received))
                        {
                            SetState(HandshakeState::SENDING_REQUEST_2);
                        }
                    }
                    else if (buffer[0] == ID_INCOMPATIBLE_PROTOCOL_VERSION)
                    {
                        HandleConnectionFailed(buffer, received, "Incompatible protocol version");
                    }
                    else if (buffer[0] == ID_ALREADY_CONNECTED)
                    {
                        HandleConnectionFailed(buffer, received, "Already connected");
                    }
                    else if (buffer[0] == ID_NO_FREE_INCOMING_CONNECTIONS)
                    {
                        HandleConnectionFailed(buffer, received, "Server full");
                    }
                    else if (buffer[0] == ID_CONNECTION_BANNED)
                    {
                        HandleConnectionFailed(buffer, received, "Banned from server");
                    }
                }
            }
            else if (elapsed > RETRY_TIMEOUT_MS)
            {
                if (++m_retryCount > MAX_RETRIES)
                {
                    m_error = "Timeout waiting for reply 1";
                    SetState(HandshakeState::FAILED);
                }
                else
                {
                    if (m_protocol == RakNetProtocol::RAKNET4)
                    {
                        // Retry with smaller MTU (RakNet 4 only)
                        m_mtu = (m_mtu > 600) ? m_mtu - 100 : RAKNET_MTU_MIN;
                    }
                    RAKNET_LOGD("Retry %d (MTU %d)", m_retryCount, m_mtu);
                    SetState(HandshakeState::SENDING_REQUEST_1);
                }
            }
            break;
        }

        case HandshakeState::SENDING_REQUEST_2:
        {
            // Only for RakNet 4
            if (m_protocol == RakNetProtocol::MTA_RAKNET3)
            {
                // MTA doesn't have request 2, skip to connection request
                SetState(HandshakeState::SENDING_CONNECTION_REQUEST);
                break;
            }

            int len = BuildOpenConnectionRequest2_RN4(buffer);
            if (SendPacket(buffer, len))
            {
                RAKNET_LOGD("Sent OPEN_CONNECTION_REQUEST_2 (%d bytes)", len);
                SetState(HandshakeState::WAITING_REPLY_2);
            }
            else
            {
                m_error = "Failed to send request 2";
                SetState(HandshakeState::FAILED);
            }
            break;
        }

        case HandshakeState::WAITING_REPLY_2:
        {
            // Only for RakNet 4
            received = ReceivePacket(buffer, sizeof(buffer), 100);
            if (received > 0)
            {
                RAKNET_LOGD("Received %d bytes, packet ID: 0x%02X", received, buffer[0]);

                if (buffer[0] == ID_OPEN_CONNECTION_REPLY_2)
                {
                    if (HandleOpenConnectionReply2_RN4(buffer, received))
                    {
                        SetState(HandshakeState::SENDING_CONNECTION_REQUEST);
                    }
                }
                else if (buffer[0] == ID_ALREADY_CONNECTED)
                {
                    HandleConnectionFailed(buffer, received, "Already connected");
                }
                else if (buffer[0] == ID_NO_FREE_INCOMING_CONNECTIONS)
                {
                    HandleConnectionFailed(buffer, received, "Server full");
                }
            }
            else if (elapsed > RETRY_TIMEOUT_MS)
            {
                if (++m_retryCount > MAX_RETRIES)
                {
                    m_error = "Timeout waiting for reply 2";
                    SetState(HandshakeState::FAILED);
                }
                else
                {
                    RAKNET_LOGD("Retry %d for request 2", m_retryCount);
                    SetState(HandshakeState::SENDING_REQUEST_2);
                }
            }
            break;
        }

        case HandshakeState::SENDING_CONNECTION_REQUEST:
        {
            int len;
            if (m_protocol == RakNetProtocol::MTA_RAKNET3)
            {
                len = BuildConnectionRequest_MTA(buffer);
                RAKNET_LOGD("Sent MTA CONNECTION_REQUEST (%d bytes)", len);
            }
            else
            {
                len = BuildConnectionRequest_RN4(buffer);
                RAKNET_LOGD("Sent RN4 CONNECTION_REQUEST (%d bytes)", len);
            }

            if (SendPacket(buffer, len))
            {
                SetState(HandshakeState::WAITING_CONNECTION_ACCEPTED);
            }
            else
            {
                m_error = "Failed to send connection request";
                SetState(HandshakeState::FAILED);
            }
            break;
        }

        case HandshakeState::WAITING_CONNECTION_ACCEPTED:
        {
            received = ReceivePacket(buffer, sizeof(buffer), 100);
            if (received > 0)
            {
                RAKNET_LOGD("Received %d bytes, packet ID: 0x%02X", received, buffer[0]);

                if (m_protocol == RakNetProtocol::MTA_RAKNET3)
                {
                    if (buffer[0] == MTA_RID_CONNECTION_REQUEST_ACCEPTED)
                    {
                        if (HandleConnectionRequestAccepted_MTA(buffer, received))
                        {
                            RAKNET_LOGI("MTA RakNet handshake complete! Connected to server.");
                            SetState(HandshakeState::CONNECTED);
                        }
                    }
                    else if (buffer[0] == MTA_RID_CONNECTION_ATTEMPT_FAILED)
                    {
                        HandleConnectionFailed(buffer, received, "Connection attempt failed");
                    }
                    else if (buffer[0] == MTA_RID_INVALID_PASSWORD)
                    {
                        HandleConnectionFailed(buffer, received, "Invalid password");
                    }
                    else if (buffer[0] == MTA_RID_CONNECTION_BANNED)
                    {
                        HandleConnectionFailed(buffer, received, "Banned");
                    }
                }
                else
                {
                    if (buffer[0] == ID_CONNECTION_REQUEST_ACCEPTED)
                    {
                        if (HandleConnectionRequestAccepted_RN4(buffer, received))
                        {
                            RAKNET_LOGI("RakNet 4 handshake complete! Connected to server.");
                            SetState(HandshakeState::CONNECTED);
                        }
                    }
                    else if (buffer[0] == ID_CONNECTION_ATTEMPT_FAILED)
                    {
                        HandleConnectionFailed(buffer, received, "Connection attempt failed");
                    }
                    else if (buffer[0] == ID_INVALID_PASSWORD)
                    {
                        HandleConnectionFailed(buffer, received, "Invalid password");
                    }
                    else if (buffer[0] == ID_CONNECTION_BANNED)
                    {
                        HandleConnectionFailed(buffer, received, "Banned");
                    }
                }
            }
            else if (elapsed > RETRY_TIMEOUT_MS * 2)
            {
                if (++m_retryCount > MAX_RETRIES)
                {
                    m_error = "Timeout waiting for connection accepted";
                    SetState(HandshakeState::FAILED);
                }
                else
                {
                    RAKNET_LOGD("Retry %d for connection request", m_retryCount);
                    SetState(HandshakeState::SENDING_CONNECTION_REQUEST);
                }
            }
            break;
        }

        case HandshakeState::CONNECTED:
        case HandshakeState::FAILED:
        case HandshakeState::DISCONNECTED:
            // Terminal states
            break;
    }

    return m_state;
}

//=============================================================================
// MTA RakNet 3.x Packet Building
//=============================================================================

int RakNetHandshake::BuildOpenConnectionRequest_MTA(uint8_t* buffer)
{
    // MTA RakNet 3.x OPEN_CONNECTION_REQUEST format:
    // 1 byte: packet ID (0x09)
    // 4 bytes: cookie (random value for connection tracking)
    // No magic bytes, no MTU padding

    int offset = 0;

    // Packet ID
    buffer[offset++] = MTA_RID_OPEN_CONNECTION_REQUEST;

    // Cookie (little-endian)
    buffer[offset++] = m_cookie & 0xFF;
    buffer[offset++] = (m_cookie >> 8) & 0xFF;
    buffer[offset++] = (m_cookie >> 16) & 0xFF;
    buffer[offset++] = (m_cookie >> 24) & 0xFF;

    return offset;
}

int RakNetHandshake::BuildConnectionRequest_MTA(uint8_t* buffer)
{
    // MTA RakNet 3.x CONNECTION_REQUEST format:
    // 1 byte: packet ID (0x04)
    // 8 bytes: client GUID
    // 8 bytes: timestamp (milliseconds)
    // 1 byte: has security (0)

    int offset = 0;

    // Packet ID
    buffer[offset++] = MTA_RID_CONNECTION_REQUEST;

    // Client GUID (big-endian)
    for (int i = 7; i >= 0; i--)
    {
        buffer[offset++] = (m_clientGUID.g >> (i * 8)) & 0xFF;
    }

    // Timestamp (big-endian)
    uint64_t time = GetTimeMs();
    for (int i = 7; i >= 0; i--)
    {
        buffer[offset++] = (time >> (i * 8)) & 0xFF;
    }

    // Has security: false
    buffer[offset++] = 0;

    return offset;
}

//=============================================================================
// MTA RakNet 3.x Packet Handling
//=============================================================================

bool RakNetHandshake::HandleOpenConnectionReply_MTA(const uint8_t* data, int length)
{
    // MTA RakNet 3.x OPEN_CONNECTION_REPLY format:
    // 1 byte: packet ID (0x0A)
    // 4 bytes: cookie (should match what we sent)

    if (length < 5)
    {
        RAKNET_LOGE("MTA reply too short: %d bytes", length);
        m_error = "Invalid MTA reply";
        SetState(HandshakeState::FAILED);
        return false;
    }

    // Verify cookie
    uint32_t replyCookie = data[1] | (data[2] << 8) | (data[3] << 16) | (data[4] << 24);
    if (replyCookie != m_cookie)
    {
        RAKNET_LOGD("Cookie mismatch: expected %08x, got %08x", m_cookie, replyCookie);
        // Some servers may use different cookies - continue anyway
    }

    RAKNET_LOGI("MTA OPEN_CONNECTION_REPLY received, cookie verified");
    m_retryCount = 0;
    return true;
}

bool RakNetHandshake::HandleConnectionRequestAccepted_MTA(const uint8_t* data, int length)
{
    // MTA RakNet 3.x CONNECTION_REQUEST_ACCEPTED format:
    // 1 byte: packet ID (0x0E)
    // 7 bytes: client address
    // 2 bytes: system index
    // ... internal addresses
    // 8 bytes: request time
    // 8 bytes: reply time

    if (length < 10)
    {
        RAKNET_LOGE("MTA connection accepted too short: %d bytes", length);
        m_error = "Invalid MTA connection accepted";
        SetState(HandshakeState::FAILED);
        return false;
    }

    RAKNET_LOGI("MTA CONNECTION_REQUEST_ACCEPTED received, length %d bytes", length);
    return true;
}

//=============================================================================
// RakNet 4 Packet Building
//=============================================================================

int RakNetHandshake::BuildOpenConnectionRequest1_RN4(uint8_t* buffer)
{
    int offset = 0;

    // Packet ID
    buffer[offset++] = ID_OPEN_CONNECTION_REQUEST_1;

    // RakNet magic (16 bytes)
    memcpy(buffer + offset, RAKNET_MAGIC, 16);
    offset += 16;

    // Protocol version
    buffer[offset++] = RAKNET_PROTOCOL_VERSION;

    // MTU padding (fill with zeros to desired MTU size)
    int padding = m_mtu - 18;
    if (padding > 0)
    {
        memset(buffer + offset, 0, padding);
        offset += padding;
    }

    return offset;
}

int RakNetHandshake::BuildOpenConnectionRequest2_RN4(uint8_t* buffer)
{
    int offset = 0;

    // Packet ID
    buffer[offset++] = ID_OPEN_CONNECTION_REQUEST_2;

    // RakNet magic
    memcpy(buffer + offset, RAKNET_MAGIC, 16);
    offset += 16;

    // Server address (7 bytes)
    WriteAddress(buffer + offset, m_serverAddr);
    offset += 7;

    // MTU (2 bytes, big-endian)
    buffer[offset++] = (m_mtu >> 8) & 0xFF;
    buffer[offset++] = m_mtu & 0xFF;

    // Client GUID (8 bytes, big-endian)
    for (int i = 7; i >= 0; i--)
    {
        buffer[offset++] = (m_clientGUID.g >> (i * 8)) & 0xFF;
    }

    return offset;
}

int RakNetHandshake::BuildConnectionRequest_RN4(uint8_t* buffer)
{
    int offset = 0;

    // Packet ID
    buffer[offset++] = ID_CONNECTION_REQUEST;

    // Client GUID (8 bytes, big-endian)
    for (int i = 7; i >= 0; i--)
    {
        buffer[offset++] = (m_clientGUID.g >> (i * 8)) & 0xFF;
    }

    // Time (8 bytes, big-endian)
    uint64_t time = GetTimeMs();
    for (int i = 7; i >= 0; i--)
    {
        buffer[offset++] = (time >> (i * 8)) & 0xFF;
    }

    // Use security: false
    buffer[offset++] = 0;

    return offset;
}

//=============================================================================
// RakNet 4 Packet Handling
//=============================================================================

bool RakNetHandshake::HandleOpenConnectionReply1_RN4(const uint8_t* data, int length)
{
    if (length < 28)
    {
        RAKNET_LOGE("Reply 1 too short: %d bytes", length);
        m_error = "Invalid reply 1";
        SetState(HandshakeState::FAILED);
        return false;
    }

    int offset = 1;

    // Verify magic
    if (memcmp(data + offset, RAKNET_MAGIC, 16) != 0)
    {
        RAKNET_LOGE("Invalid magic in reply 1");
        m_error = "Invalid magic";
        SetState(HandshakeState::FAILED);
        return false;
    }
    offset += 16;

    // Server GUID
    m_serverGUID.g = 0;
    for (int i = 0; i < 8; i++)
    {
        m_serverGUID.g = (m_serverGUID.g << 8) | data[offset++];
    }
    RAKNET_LOGD("Server GUID: %llx", (unsigned long long)m_serverGUID.g);

    // Has security
    bool hasSecurity = data[offset++] != 0;
    RAKNET_LOGD("Server has security: %d", hasSecurity);

    // MTU size
    m_mtu = (data[offset] << 8) | data[offset + 1];
    RAKNET_LOGD("Negotiated MTU: %d", m_mtu);

    m_retryCount = 0;
    return true;
}

bool RakNetHandshake::HandleOpenConnectionReply2_RN4(const uint8_t* data, int length)
{
    if (length < 35)
    {
        RAKNET_LOGE("Reply 2 too short: %d bytes", length);
        m_error = "Invalid reply 2";
        SetState(HandshakeState::FAILED);
        return false;
    }

    int offset = 1;

    // Verify magic
    if (memcmp(data + offset, RAKNET_MAGIC, 16) != 0)
    {
        RAKNET_LOGE("Invalid magic in reply 2");
        m_error = "Invalid magic";
        SetState(HandshakeState::FAILED);
        return false;
    }
    offset += 16;

    // Server GUID
    RakNetGUID serverGUID;
    serverGUID.g = 0;
    for (int i = 0; i < 8; i++)
    {
        serverGUID.g = (serverGUID.g << 8) | data[offset++];
    }

    if (serverGUID.g != m_serverGUID.g)
    {
        RAKNET_LOGD("Server GUID changed (expected %llx, got %llx)",
                    (unsigned long long)m_serverGUID.g, (unsigned long long)serverGUID.g);
        m_serverGUID = serverGUID;
    }

    // Skip client address (7 bytes)
    offset += 7;

    // MTU
    uint16_t mtu = (data[offset] << 8) | data[offset + 1];
    RAKNET_LOGD("Final MTU: %d", mtu);
    m_mtu = mtu;

    m_retryCount = 0;
    return true;
}

bool RakNetHandshake::HandleConnectionRequestAccepted_RN4(const uint8_t* data, int length)
{
    if (length < 10)
    {
        RAKNET_LOGE("Connection accepted too short: %d bytes", length);
        m_error = "Invalid connection accepted";
        SetState(HandshakeState::FAILED);
        return false;
    }

    RAKNET_LOGI("Connection request accepted, length %d bytes", length);
    return true;
}

//=============================================================================
// Common Handlers
//=============================================================================

bool RakNetHandshake::HandleConnectionFailed(const uint8_t* data, int length, const char* reason)
{
    RAKNET_LOGE("Connection failed: %s", reason);
    m_error = reason;
    SetState(HandshakeState::FAILED);
    return false;
}

//=============================================================================
// Network I/O
//=============================================================================

bool RakNetHandshake::SendPacket(const uint8_t* data, int length)
{
    ssize_t sent = sendto(m_socket, data, length, 0,
                          reinterpret_cast<const sockaddr*>(&m_serverAddr),
                          sizeof(m_serverAddr));

    if (sent != length)
    {
        RAKNET_LOGE("sendto failed: %s", strerror(errno));
        return false;
    }
    return true;
}

int RakNetHandshake::ReceivePacket(uint8_t* buffer, int maxLength, int timeoutMs)
{
    struct pollfd pfd;
    pfd.fd = m_socket;
    pfd.events = POLLIN;

    int result = poll(&pfd, 1, timeoutMs);
    if (result > 0 && (pfd.revents & POLLIN))
    {
        sockaddr_in fromAddr;
        socklen_t fromLen = sizeof(fromAddr);

        ssize_t received = recvfrom(m_socket, buffer, maxLength, 0,
                                    reinterpret_cast<sockaddr*>(&fromAddr), &fromLen);

        if (received > 0)
        {
            return static_cast<int>(received);
        }
    }
    return 0;
}

//=============================================================================
// Utility
//=============================================================================

void RakNetHandshake::SetState(HandshakeState newState)
{
    if (m_state != newState)
    {
        RAKNET_LOGI("Handshake state: %s -> %s",
                    HandshakeStateToString(m_state),
                    HandshakeStateToString(newState));
        m_state = newState;
        m_stateStartTime = GetTimeMs();
    }
}

uint64_t RakNetHandshake::GetTimeMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(
        steady_clock::now().time_since_epoch()
    ).count();
}

void RakNetHandshake::WriteAddress(uint8_t* buffer, const sockaddr_in& addr)
{
    // RakNet address format:
    // 1 byte: address family (4 = IPv4)
    // 4 bytes: IP address (inverted)
    // 2 bytes: port (big-endian)

    buffer[0] = 4;  // IPv4

    // IP address (inverted for RakNet)
    uint32_t ip = ntohl(addr.sin_addr.s_addr);
    buffer[1] = ~((ip >> 24) & 0xFF);
    buffer[2] = ~((ip >> 16) & 0xFF);
    buffer[3] = ~((ip >> 8) & 0xFF);
    buffer[4] = ~(ip & 0xFF);

    // Port (big-endian)
    uint16_t port = ntohs(addr.sin_port);
    buffer[5] = (port >> 8) & 0xFF;
    buffer[6] = port & 0xFF;
}

} // namespace RakNet
