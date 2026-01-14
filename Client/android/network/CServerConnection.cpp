/*
 * MTA:SA Android - Server Connection Implementation
 *
 * Phase 7: Multiplayer Logic
 */

#include "CServerConnection.h"
#include "SyncStructures.h"
#include <android/log.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <cmath>
#include <cstring>
#include <random>
#include <sstream>
#include <iomanip>
#include <vector>

#define LOG_TAG "MTA-Connection"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace MTA::Android::Network
{

namespace
{
SControllerState BuildControllerFromVelocity(float vx, float vy, float rotation)
{
    SControllerState state;

    const float speed = std::sqrt(vx * vx + vy * vy);
    if (speed < 0.01f)
        return state;

    constexpr float MAX_SPEED = 4.5f;
    constexpr float RUN_THRESHOLD = 3.2f;

    float moveHeading = std::atan2(-vx, vy);
    float relative = moveHeading - rotation;
    relative = WrapAngle(relative);

    float localX = std::sin(relative);
    float localY = std::cos(relative);

    float factor = speed / MAX_SPEED;
    if (factor > 1.0f) factor = 1.0f;

    state.LeftStickX = static_cast<int16_t>(std::lround(localX * 128.0f * factor));
    state.LeftStickY = static_cast<int16_t>(std::lround(-localY * 128.0f * factor));

    if (speed > RUN_THRESHOLD)
    {
        state.ButtonCross = 255;  // Sprint
    }
    else
    {
        state.m_bPedWalk = 1;  // Walk
    }

    return state;
}

void WriteCameraOrientationPlaceholder(NetBitStream& bs, float camRotation)
{
    // Minimal placeholder that preserves bitstream alignment for MTA PC format.
    WriteFloatAsBits(bs, 8, -PI, PI, camRotation, false);  // Cam rot Z
    WriteFloatAsBits(bs, 8, -PI, PI, 0.0f, false);         // Cam rot X

    // Offset section: use relative position with smallest range and zero offsets.
    bs.WriteBit(false);  // bUseAbsolutePosition
    uint8_t idx = 0;
    bs.WriteBits(&idx, 2);

    WriteFloatAsBits(bs, 3, -4.0f, 4.0f, 0.0f, false);
    WriteFloatAsBits(bs, 3, -4.0f, 4.0f, 0.0f, false);
    WriteFloatAsBits(bs, 3, -4.0f, 4.0f, 0.0f, false);
}
} // namespace

//=============================================================================
// MD5 Implementation
//=============================================================================

// Simple MD5 implementation for password hashing
// Based on RFC 1321

namespace
{

struct MD5Context
{
    uint32_t state[4];
    uint32_t count[2];
    uint8_t buffer[64];
};

constexpr uint32_t S11 = 7, S12 = 12, S13 = 17, S14 = 22;
constexpr uint32_t S21 = 5, S22 = 9, S23 = 14, S24 = 20;
constexpr uint32_t S31 = 4, S32 = 11, S33 = 16, S34 = 23;
constexpr uint32_t S41 = 6, S42 = 10, S43 = 15, S44 = 21;

inline uint32_t F(uint32_t x, uint32_t y, uint32_t z) { return (x & y) | (~x & z); }
inline uint32_t G(uint32_t x, uint32_t y, uint32_t z) { return (x & z) | (y & ~z); }
inline uint32_t H(uint32_t x, uint32_t y, uint32_t z) { return x ^ y ^ z; }
inline uint32_t I(uint32_t x, uint32_t y, uint32_t z) { return y ^ (x | ~z); }
inline uint32_t ROTATE_LEFT(uint32_t x, uint32_t n) { return (x << n) | (x >> (32 - n)); }

inline void FF(uint32_t& a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t ac)
{
    a += F(b, c, d) + x + ac;
    a = ROTATE_LEFT(a, s) + b;
}

inline void GG(uint32_t& a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t ac)
{
    a += G(b, c, d) + x + ac;
    a = ROTATE_LEFT(a, s) + b;
}

inline void HH(uint32_t& a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t ac)
{
    a += H(b, c, d) + x + ac;
    a = ROTATE_LEFT(a, s) + b;
}

inline void II(uint32_t& a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t ac)
{
    a += I(b, c, d) + x + ac;
    a = ROTATE_LEFT(a, s) + b;
}

void MD5Transform(uint32_t state[4], const uint8_t block[64])
{
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t x[16];

    for (int i = 0; i < 16; i++)
    {
        x[i] = static_cast<uint32_t>(block[i * 4]) |
               (static_cast<uint32_t>(block[i * 4 + 1]) << 8) |
               (static_cast<uint32_t>(block[i * 4 + 2]) << 16) |
               (static_cast<uint32_t>(block[i * 4 + 3]) << 24);
    }

    // Round 1
    FF(a, b, c, d, x[0], S11, 0xd76aa478);
    FF(d, a, b, c, x[1], S12, 0xe8c7b756);
    FF(c, d, a, b, x[2], S13, 0x242070db);
    FF(b, c, d, a, x[3], S14, 0xc1bdceee);
    FF(a, b, c, d, x[4], S11, 0xf57c0faf);
    FF(d, a, b, c, x[5], S12, 0x4787c62a);
    FF(c, d, a, b, x[6], S13, 0xa8304613);
    FF(b, c, d, a, x[7], S14, 0xfd469501);
    FF(a, b, c, d, x[8], S11, 0x698098d8);
    FF(d, a, b, c, x[9], S12, 0x8b44f7af);
    FF(c, d, a, b, x[10], S13, 0xffff5bb1);
    FF(b, c, d, a, x[11], S14, 0x895cd7be);
    FF(a, b, c, d, x[12], S11, 0x6b901122);
    FF(d, a, b, c, x[13], S12, 0xfd987193);
    FF(c, d, a, b, x[14], S13, 0xa679438e);
    FF(b, c, d, a, x[15], S14, 0x49b40821);

    // Round 2
    GG(a, b, c, d, x[1], S21, 0xf61e2562);
    GG(d, a, b, c, x[6], S22, 0xc040b340);
    GG(c, d, a, b, x[11], S23, 0x265e5a51);
    GG(b, c, d, a, x[0], S24, 0xe9b6c7aa);
    GG(a, b, c, d, x[5], S21, 0xd62f105d);
    GG(d, a, b, c, x[10], S22, 0x02441453);
    GG(c, d, a, b, x[15], S23, 0xd8a1e681);
    GG(b, c, d, a, x[4], S24, 0xe7d3fbc8);
    GG(a, b, c, d, x[9], S21, 0x21e1cde6);
    GG(d, a, b, c, x[14], S22, 0xc33707d6);
    GG(c, d, a, b, x[3], S23, 0xf4d50d87);
    GG(b, c, d, a, x[8], S24, 0x455a14ed);
    GG(a, b, c, d, x[13], S21, 0xa9e3e905);
    GG(d, a, b, c, x[2], S22, 0xfcefa3f8);
    GG(c, d, a, b, x[7], S23, 0x676f02d9);
    GG(b, c, d, a, x[12], S24, 0x8d2a4c8a);

    // Round 3
    HH(a, b, c, d, x[5], S31, 0xfffa3942);
    HH(d, a, b, c, x[8], S32, 0x8771f681);
    HH(c, d, a, b, x[11], S33, 0x6d9d6122);
    HH(b, c, d, a, x[14], S34, 0xfde5380c);
    HH(a, b, c, d, x[1], S31, 0xa4beea44);
    HH(d, a, b, c, x[4], S32, 0x4bdecfa9);
    HH(c, d, a, b, x[7], S33, 0xf6bb4b60);
    HH(b, c, d, a, x[10], S34, 0xbebfbc70);
    HH(a, b, c, d, x[13], S31, 0x289b7ec6);
    HH(d, a, b, c, x[0], S32, 0xeaa127fa);
    HH(c, d, a, b, x[3], S33, 0xd4ef3085);
    HH(b, c, d, a, x[6], S34, 0x04881d05);
    HH(a, b, c, d, x[9], S31, 0xd9d4d039);
    HH(d, a, b, c, x[12], S32, 0xe6db99e5);
    HH(c, d, a, b, x[15], S33, 0x1fa27cf8);
    HH(b, c, d, a, x[2], S34, 0xc4ac5665);

    // Round 4
    II(a, b, c, d, x[0], S41, 0xf4292244);
    II(d, a, b, c, x[7], S42, 0x432aff97);
    II(c, d, a, b, x[14], S43, 0xab9423a7);
    II(b, c, d, a, x[5], S44, 0xfc93a039);
    II(a, b, c, d, x[12], S41, 0x655b59c3);
    II(d, a, b, c, x[3], S42, 0x8f0ccc92);
    II(c, d, a, b, x[10], S43, 0xffeff47d);
    II(b, c, d, a, x[1], S44, 0x85845dd1);
    II(a, b, c, d, x[8], S41, 0x6fa87e4f);
    II(d, a, b, c, x[15], S42, 0xfe2ce6e0);
    II(c, d, a, b, x[6], S43, 0xa3014314);
    II(b, c, d, a, x[13], S44, 0x4e0811a1);
    II(a, b, c, d, x[4], S41, 0xf7537e82);
    II(d, a, b, c, x[11], S42, 0xbd3af235);
    II(c, d, a, b, x[2], S43, 0x2ad7d2bb);
    II(b, c, d, a, x[9], S44, 0xeb86d391);

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
}

} // anonymous namespace

void MD5::Compute(const uint8_t* data, size_t length, uint8_t output[16])
{
    MD5Context ctx;
    ctx.state[0] = 0x67452301;
    ctx.state[1] = 0xefcdab89;
    ctx.state[2] = 0x98badcfe;
    ctx.state[3] = 0x10325476;
    ctx.count[0] = ctx.count[1] = 0;

    // Process input in 64-byte chunks
    size_t index = 0;
    size_t partLen = 64 - (ctx.count[0] >> 3) % 64;

    ctx.count[0] += static_cast<uint32_t>(length << 3);
    if (ctx.count[0] < static_cast<uint32_t>(length << 3))
        ctx.count[1]++;
    ctx.count[1] += static_cast<uint32_t>(length >> 29);

    size_t i = 0;
    if (length >= partLen)
    {
        std::memcpy(&ctx.buffer[(ctx.count[0] >> 3) % 64], data, partLen);
        MD5Transform(ctx.state, ctx.buffer);

        for (i = partLen; i + 63 < length; i += 64)
            MD5Transform(ctx.state, &data[i]);

        index = 0;
    }

    std::memcpy(&ctx.buffer[index], &data[i], length - i);

    // Padding
    static const uint8_t padding[64] = {
        0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    uint8_t bits[8];
    for (int j = 0; j < 4; j++)
    {
        bits[j] = static_cast<uint8_t>(ctx.count[0] >> (j * 8));
        bits[j + 4] = static_cast<uint8_t>(ctx.count[1] >> (j * 8));
    }

    size_t padLen = ((ctx.count[0] >> 3) % 64 < 56) ?
                    (56 - (ctx.count[0] >> 3) % 64) :
                    (120 - (ctx.count[0] >> 3) % 64);

    // Final padding and length
    uint8_t finalBlock[128];
    size_t bufIndex = (ctx.count[0] >> 3) % 64;
    std::memcpy(finalBlock, ctx.buffer, bufIndex);
    std::memcpy(finalBlock + bufIndex, padding, padLen);
    std::memcpy(finalBlock + bufIndex + padLen, bits, 8);

    size_t totalLen = bufIndex + padLen + 8;
    for (size_t j = 0; j < totalLen; j += 64)
        MD5Transform(ctx.state, finalBlock + j);

    // Output
    for (int j = 0; j < 4; j++)
    {
        output[j * 4] = static_cast<uint8_t>(ctx.state[j]);
        output[j * 4 + 1] = static_cast<uint8_t>(ctx.state[j] >> 8);
        output[j * 4 + 2] = static_cast<uint8_t>(ctx.state[j] >> 16);
        output[j * 4 + 3] = static_cast<uint8_t>(ctx.state[j] >> 24);
    }
}

void MD5::Compute(const std::string& str, uint8_t output[16])
{
    Compute(reinterpret_cast<const uint8_t*>(str.data()), str.length(), output);
}

//=============================================================================
// CServerConnection Implementation
//=============================================================================

CServerConnection::CServerConnection()
{
    LOGD("CServerConnection created");
}

CServerConnection::~CServerConnection()
{
    Shutdown();
    LOGD("CServerConnection destroyed");
}

bool CServerConnection::Initialize()
{
    LOGI("Initializing server connection system");

    // Use the CNetAndroid singleton
    m_network = &CNetAndroid::Instance();
    if (!m_network->Initialize())
    {
        LOGE("Failed to initialize network");
        return false;
    }

    m_state = ServerConnectionState::DISCONNECTED;
    return true;
}

void CServerConnection::Shutdown()
{
    if (m_state != ServerConnectionState::DISCONNECTED)
    {
        Disconnect("Shutdown");
    }

    if (m_socket >= 0)
    {
        close(m_socket);
        m_socket = -1;
    }

    // Don't shutdown the singleton, just clear our reference
    m_network = nullptr;
}

void CServerConnection::SetCallbacks(const ConnectionCallbacks& callbacks)
{
    m_callbacks = callbacks;
}

const char* CServerConnection::StateToString(ServerConnectionState state)
{
    switch (state)
    {
        case ServerConnectionState::DISCONNECTED:       return "DISCONNECTED";
        case ServerConnectionState::RESOLVING_DNS:      return "RESOLVING_DNS";
        case ServerConnectionState::CONNECTING:         return "CONNECTING";
        case ServerConnectionState::RAKNET_HANDSHAKE:   return "RAKNET_HANDSHAKE";
        case ServerConnectionState::WAIT_MOD_NAME:      return "WAIT_MOD_NAME";
        case ServerConnectionState::SENDING_JOIN:       return "SENDING_JOIN";
        case ServerConnectionState::WAIT_JOIN_COMPLETE: return "WAIT_JOIN_COMPLETE";
        case ServerConnectionState::WAIT_JOINED_GAME:   return "WAIT_JOINED_GAME";
        case ServerConnectionState::CONNECTED:          return "CONNECTED";
        case ServerConnectionState::DISCONNECTING:      return "DISCONNECTING";
        case ServerConnectionState::ERROR_STATE:        return "ERROR";
        default:                                        return "UNKNOWN";
    }
}

void CServerConnection::SetState(ServerConnectionState newState, const std::string& message)
{
    ServerConnectionState oldState = m_state.load();
    if (oldState == newState)
        return;

    LOGI("Connection state: %s -> %s%s%s",
         StateToString(oldState),
         StateToString(newState),
         message.empty() ? "" : " (",
         message.empty() ? "" : message.c_str());

    m_state = newState;
    m_stateStartTime = GetCurrentTimeMs();

    if (m_callbacks.onStateChanged)
    {
        m_callbacks.onStateChanged(newState, message);
    }
}

uint64_t CServerConnection::GetCurrentTimeMs()
{
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
}

bool CServerConnection::Connect(const ServerInfo& server, const PlayerInfo& player)
{
    if (m_state != ServerConnectionState::DISCONNECTED)
    {
        LOGE("Cannot connect: already connected or connecting");
        return false;
    }

    // Validate inputs
    if (server.host.empty())
    {
        LOGE("Invalid server host");
        return false;
    }

    if (player.nickname.empty() || player.nickname.length() > MAX_PLAYER_NICK_LENGTH)
    {
        LOGE("Invalid nickname (must be 1-%zu characters)", MAX_PLAYER_NICK_LENGTH);
        return false;
    }

    // Reserved nicknames
    if (player.nickname == "admin" || player.nickname == "console" || player.nickname == "server")
    {
        LOGE("Reserved nickname: %s", player.nickname.c_str());
        return false;
    }

    m_serverInfo = server;
    m_playerInfo = player;
    m_retryCount = 0;

    LOGI("Connecting to %s:%d as '%s'",
         server.host.c_str(), server.port, player.nickname.c_str());

    SetState(ServerConnectionState::RESOLVING_DNS, "Resolving hostname");
    return true;
}

void CServerConnection::Disconnect(const std::string& reason)
{
    if (m_state == ServerConnectionState::DISCONNECTED)
        return;

    LOGI("Disconnecting: %s", reason.empty() ? "user request" : reason.c_str());

    SetState(ServerConnectionState::DISCONNECTING, reason);

    // Send disconnect packet if connected
    if (m_socket >= 0)
    {
        SendDisconnectPacket();
        close(m_socket);
        m_socket = -1;
    }

    SetState(ServerConnectionState::DISCONNECTED, reason);

    if (m_callbacks.onDisconnected)
    {
        m_callbacks.onDisconnected(reason);
    }
}

void CServerConnection::Process()
{
    ProcessState();
}

void CServerConnection::ProcessState()
{
    uint64_t now = GetCurrentTimeMs();
    uint64_t elapsed = now - m_stateStartTime;

    switch (m_state.load())
    {
        case ServerConnectionState::RESOLVING_DNS:
            ProcessResolvingDNS();
            break;

        case ServerConnectionState::CONNECTING:
            ProcessConnecting();
            break;

        case ServerConnectionState::RAKNET_HANDSHAKE:
            ProcessRakNetHandshake();
            break;

        case ServerConnectionState::WAIT_MOD_NAME:
            ProcessWaitModName();
            break;

        case ServerConnectionState::SENDING_JOIN:
            ProcessSendingJoin();
            break;

        case ServerConnectionState::WAIT_JOIN_COMPLETE:
            ProcessWaitJoinComplete();
            break;

        case ServerConnectionState::WAIT_JOINED_GAME:
            ProcessWaitJoinedGame();
            break;

        case ServerConnectionState::CONNECTED:
            ProcessConnected();
            break;

        default:
            break;
    }

    // Check for timeout
    if (m_state != ServerConnectionState::DISCONNECTED &&
        m_state != ServerConnectionState::CONNECTED &&
        m_state != ServerConnectionState::ERROR_STATE)
    {
        if (elapsed > m_connectionTimeoutMs)
        {
            LOGE("Connection timeout in state %s", StateToString(m_state));

            if (++m_retryCount < MAX_RETRIES)
            {
                LOGI("Retrying connection (attempt %d/%d)", m_retryCount + 1, MAX_RETRIES);
                SetState(ServerConnectionState::CONNECTING, "Retrying");
            }
            else
            {
                SetState(ServerConnectionState::ERROR_STATE, "Connection timeout");
                if (m_callbacks.onError)
                {
                    m_callbacks.onError("Connection timeout after " +
                                       std::to_string(MAX_RETRIES) + " attempts");
                }
            }
        }
    }
}

void CServerConnection::ProcessResolvingDNS()
{
    m_resolvedIP = TestDNSResolution(m_serverInfo.host);

    if (m_resolvedIP.empty())
    {
        LOGE("DNS resolution failed for %s", m_serverInfo.host.c_str());
        SetState(ServerConnectionState::ERROR_STATE, "DNS resolution failed");
        if (m_callbacks.onError)
        {
            m_callbacks.onError("Failed to resolve hostname: " + m_serverInfo.host);
        }
        return;
    }

    LOGI("Resolved %s to %s", m_serverInfo.host.c_str(), m_resolvedIP.c_str());
    SetState(ServerConnectionState::CONNECTING, "Creating socket");
}

void CServerConnection::ProcessConnecting()
{
    // Create UDP socket if not already created
    if (m_socket < 0)
    {
        m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (m_socket < 0)
        {
            LOGE("Failed to create socket: %s", strerror(errno));
            SetState(ServerConnectionState::ERROR_STATE, "Socket creation failed");
            return;
        }

        // Set non-blocking
        int flags = fcntl(m_socket, F_GETFL, 0);
        fcntl(m_socket, F_SETFL, flags | O_NONBLOCK);

        // Set socket options
        int recvBufSize = 65536;
        setsockopt(m_socket, SOL_SOCKET, SO_RCVBUF, &recvBufSize, sizeof(recvBufSize));
    }

    // Create server address for handshake
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(m_serverInfo.port);
    inet_pton(AF_INET, m_resolvedIP.c_str(), &serverAddr.sin_addr);

    // Create RakNet handshake and start it
    m_raknetHandshake = std::make_unique<RakNet::RakNetHandshake>();
    if (!m_raknetHandshake->Start(m_socket, serverAddr))
    {
        LOGE("Failed to start RakNet handshake");
        SetState(ServerConnectionState::ERROR_STATE, "RakNet handshake start failed");
        return;
    }

    LOGI("Socket created, starting RakNet handshake");
    SetState(ServerConnectionState::RAKNET_HANDSHAKE, "RakNet handshake");
}

void CServerConnection::ProcessRakNetHandshake()
{
    if (!m_raknetHandshake)
    {
        LOGE("RakNet handshake not initialized");
        SetState(ServerConnectionState::ERROR_STATE, "Internal error");
        return;
    }

    // Process handshake state machine
    RakNet::HandshakeState state = m_raknetHandshake->Process();

    switch (state)
    {
        case RakNet::HandshakeState::CONNECTED:
            LOGI("RakNet handshake completed! Server GUID: %llu, MTU: %d",
                 (unsigned long long)m_raknetHandshake->GetServerGUID().g,
                 m_raknetHandshake->GetMTU());
            SetState(ServerConnectionState::WAIT_MOD_NAME, "RakNet connected");
            break;

        case RakNet::HandshakeState::FAILED:
            LOGE("RakNet handshake failed: %s", m_raknetHandshake->GetError().c_str());
            SetState(ServerConnectionState::ERROR_STATE, "RakNet handshake failed: " + m_raknetHandshake->GetError());
            if (m_callbacks.onError)
            {
                m_callbacks.onError("RakNet handshake failed: " + m_raknetHandshake->GetError());
            }
            break;

        default:
            // Still in progress - state machine handles retries internally
            break;
    }
}

void CServerConnection::SendConnectionRequest()
{
    // Send a simple connection request packet
    // This triggers the test server to respond with MOD_NAME
    // In real MTA, this would be RakNet's OPEN_CONNECTION_REQUEST_1

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(m_serverInfo.port);
    inet_pton(AF_INET, m_resolvedIP.c_str(), &serverAddr.sin_addr);

    // Simple MTA-style connection request
    // Format: "MTA" + protocol version + client type
    uint8_t packet[16];
    packet[0] = 'M';
    packet[1] = 'T';
    packet[2] = 'A';
    packet[3] = 'C';  // Client
    packet[4] = (MTA_DM_VERSION >> 8) & 0xFF;
    packet[5] = MTA_DM_VERSION & 0xFF;
    packet[6] = (MTA_DM_BITSTREAM_VERSION >> 8) & 0xFF;
    packet[7] = MTA_DM_BITSTREAM_VERSION & 0xFF;

    ssize_t sent = sendto(m_socket, packet, 8, 0,
                          reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr));

    if (sent > 0)
    {
        LOGI("Sent connection request (%zd bytes) to %s:%d",
             sent, m_resolvedIP.c_str(), m_serverInfo.port);
    }
    else
    {
        LOGE("Failed to send connection request: %s", strerror(errno));
    }
}

void CServerConnection::ProcessWaitModName()
{
    // Resend connection request every 2 seconds if no response
    static uint64_t lastRequestTime = 0;
    uint64_t now = GetCurrentTimeMs();
    if (now - lastRequestTime > 2000)
    {
        LOGI("Resending connection request...");
        SendConnectionRequest();
        lastRequestTime = now;
    }

    // Poll for incoming data
    struct pollfd pfd;
    pfd.fd = m_socket;
    pfd.events = POLLIN;

    int result = poll(&pfd, 1, 100);  // 100ms timeout
    if (result > 0 && (pfd.revents & POLLIN))
    {
        uint8_t buffer[2048];
        sockaddr_in fromAddr;
        socklen_t fromLen = sizeof(fromAddr);

        ssize_t received = recvfrom(m_socket, buffer, sizeof(buffer), 0,
                                    reinterpret_cast<sockaddr*>(&fromAddr), &fromLen);

        if (received > 0)
        {
            LOGI("Received %zd bytes from server (packet ID: 0x%02X)", received, buffer[0]);
            m_lastPacketTime = GetCurrentTimeMs();

            // Check for MOD_NAME packet
            if (buffer[0] == PACKET_ID_MOD_NAME)
            {
                NetBitStream bitStream(buffer + 1, received - 1);
                HandleModNamePacket(bitStream);
            }
            else
            {
                LOGD("Unexpected packet ID: 0x%02X (expected MOD_NAME 0x%02X)",
                     buffer[0], PACKET_ID_MOD_NAME);
            }
        }
    }
}

void CServerConnection::ProcessSendingJoin()
{
    SendJoinDataPacket();
    SetState(ServerConnectionState::WAIT_JOIN_COMPLETE, "Join data sent");
}

void CServerConnection::ProcessWaitJoinComplete()
{
    // Poll for SERVER_JOIN_COMPLETE packet
    struct pollfd pfd;
    pfd.fd = m_socket;
    pfd.events = POLLIN;

    int result = poll(&pfd, 1, 100);
    if (result > 0 && (pfd.revents & POLLIN))
    {
        uint8_t buffer[2048];
        sockaddr_in fromAddr;
        socklen_t fromLen = sizeof(fromAddr);

        ssize_t received = recvfrom(m_socket, buffer, sizeof(buffer), 0,
                                    reinterpret_cast<sockaddr*>(&fromAddr), &fromLen);

        if (received > 0)
        {
            m_lastPacketTime = GetCurrentTimeMs();

            if (buffer[0] == PACKET_ID_SERVER_JOIN_COMPLETE)
            {
                NetBitStream bitStream(buffer + 1, received - 1);
                HandleJoinCompletePacket(bitStream);
            }
        }
    }
}

void CServerConnection::ProcessWaitJoinedGame()
{
    // Poll for SERVER_JOINEDGAME packet
    struct pollfd pfd;
    pfd.fd = m_socket;
    pfd.events = POLLIN;

    int result = poll(&pfd, 1, 100);
    if (result > 0 && (pfd.revents & POLLIN))
    {
        uint8_t buffer[2048];
        sockaddr_in fromAddr;
        socklen_t fromLen = sizeof(fromAddr);

        ssize_t received = recvfrom(m_socket, buffer, sizeof(buffer), 0,
                                    reinterpret_cast<sockaddr*>(&fromAddr), &fromLen);

        if (received > 0)
        {
            m_lastPacketTime = GetCurrentTimeMs();

            if (buffer[0] == PACKET_ID_SERVER_JOINEDGAME)
            {
                NetBitStream bitStream(buffer + 1, received - 1);
                HandleJoinedGamePacket(bitStream);
            }
        }
    }
}

void CServerConnection::ProcessConnected()
{
    // Receive packets directly from our connected socket (not through CNetAndroid)
    struct pollfd pfd;
    pfd.fd = m_socket;
    pfd.events = POLLIN;

    // Check for incoming packets (non-blocking)
    int result = poll(&pfd, 1, 10);  // 10ms timeout
    if (result > 0 && (pfd.revents & POLLIN))
    {
        uint8_t buffer[2048];
        sockaddr_in fromAddr;
        socklen_t fromLen = sizeof(fromAddr);

        ssize_t received = recvfrom(m_socket, buffer, sizeof(buffer), 0,
                                    reinterpret_cast<sockaddr*>(&fromAddr), &fromLen);

        if (received > 0)
        {
            m_lastPacketTime = GetCurrentTimeMs();
            uint8_t packetId = buffer[0];

            // Debug log for sync packets
            static int incomingCount = 0;
            if (packetId == 0x20)  // PLAYER_PURESYNC
            {
                if (++incomingCount % 100 == 1)
                {
                    LOGD("Received PURESYNC from server (%zd bytes)", received);
                }
            }
            else
            {
                LOGD("Received packet 0x%02X from server (%zd bytes)", packetId, received);
            }

            // Dispatch to packet handler if registered
            if (m_network && m_network->HasPacketHandler())
            {
                NetBitStream bitStream(buffer + 1, received - 1);
                m_network->DispatchPacket(static_cast<PacketID>(packetId), bitStream);
            }
        }
    }

    // Also do network pulse for stats etc (but not for packet receiving)
    if (m_network)
    {
        m_network->DoPulse();
    }
}

void CServerConnection::HandleModNamePacket(NetBitStream& bitStream)
{
    uint16_t bitstreamVersion = 0;
    std::string moduleName;

    bitStream.Read(bitstreamVersion);
    bitStream.Read(moduleName, 64);

    LOGI("MOD_NAME: version=%04X, module='%s'", bitstreamVersion, moduleName.c_str());

    if (moduleName != "deathmatch")
    {
        LOGE("Invalid module name: %s (expected 'deathmatch')", moduleName.c_str());
        SetState(ServerConnectionState::ERROR_STATE, "Invalid server module");
        return;
    }

    SetState(ServerConnectionState::SENDING_JOIN, "Module verified");
}

void CServerConnection::HandleJoinCompletePacket(NetBitStream& bitStream)
{
    uint16_t versionLength = 0;
    bitStream.Read(versionLength);

    char versionBuffer[256] = {0};
    if (versionLength > 0 && versionLength < sizeof(versionBuffer))
    {
        for (uint16_t i = 0; i < versionLength; i++)
        {
            bitStream.Read(reinterpret_cast<uint8_t&>(versionBuffer[i]));
        }
    }
    m_serverInfo.serverVersion = versionBuffer;

    std::string fullVersion;
    bitStream.Read(fullVersion, 128);

    LOGI("JOIN_COMPLETE: version='%s', full='%s'",
         m_serverInfo.serverVersion.c_str(), fullVersion.c_str());

    SetState(ServerConnectionState::WAIT_JOINED_GAME, "Join complete received");
}

void CServerConnection::HandleJoinedGamePacket(NetBitStream& bitStream)
{
    uint16_t playerId = 0;
    uint8_t playerCount = 0;
    uint16_t rootElementId = 0;

    bitStream.Read(playerId);
    bitStream.Read(playerCount);
    bitStream.Read(rootElementId);

    m_playerId = playerId;

    LOGI("JOINED_GAME: playerId=%d, rootElement=%d", playerId, rootElementId);

    SetState(ServerConnectionState::CONNECTED, "Joined game");

    // Notify callback
    if (m_callbacks.onConnected)
    {
        ConnectionResult result;
        result.success = true;
        result.finalState = ServerConnectionState::CONNECTED;
        result.playerId = m_playerId;
        result.serverName = m_serverInfo.serverName;
        result.serverVersion = m_serverInfo.serverVersion;
        m_callbacks.onConnected(result);
    }
}

void CServerConnection::SendJoinDataPacket()
{
    NetBitStream bitStream;

    // Netcode version
    bitStream.Write(MTA_DM_NETCODE_VERSION);

    // MTA version
    bitStream.Write(MTA_DM_VERSION);

    // Bitstream version
    bitStream.Write(MTA_DM_BITSTREAM_VERSION);

    // Player version string (format: major.minor.patch-type.build.revision)
    // Real MTA clients use format like "1.6.0-9.23324.0"
    // Type 9 = release builds, build number must be reasonable
    std::string versionString = "1.6.0-9.21000.0";
    bitStream.Write(versionString);

    // Update required flag (1 bit)
    bitStream.WriteBit(false);

    // Game version (GTA:SA)
    bitStream.Write(m_playerInfo.gameVersion);

    // Nickname (fixed 22 bytes)
    char nickname[MAX_PLAYER_NICK_LENGTH + 1] = {0};
    strncpy(nickname, m_playerInfo.nickname.c_str(), MAX_PLAYER_NICK_LENGTH);
    for (size_t i = 0; i < MAX_PLAYER_NICK_LENGTH; i++)
    {
        bitStream.Write(static_cast<uint8_t>(nickname[i]));
    }

    // Password MD5 hash (16 bytes)
    uint8_t passwordHash[MD5_HASH_LENGTH] = {0};
    if (!m_playerInfo.password.empty())
    {
        MD5::Compute(m_playerInfo.password, passwordHash);
    }
    for (size_t i = 0; i < MD5_HASH_LENGTH; i++)
    {
        bitStream.Write(passwordHash[i]);
    }

    // Serial (fixed 32 bytes, legacy - empty)
    std::string serial = GenerateSerial();
    char serialBuffer[MAX_SERIAL_LENGTH + 1] = {0};
    strncpy(serialBuffer, serial.c_str(), MAX_SERIAL_LENGTH);
    for (size_t i = 0; i < MAX_SERIAL_LENGTH; i++)
    {
        bitStream.Write(static_cast<uint8_t>(serialBuffer[i]));
    }

    // Send packet
    uint8_t packet[2048];
    packet[0] = PACKET_ID_PLAYER_JOINDATA;
    size_t dataSize = bitStream.GetBytesUsed();
    std::memcpy(packet + 1, bitStream.GetData(), dataSize);

    // Debug: Hex dump of JOINDATA packet
    LOGI("=== JOINDATA PACKET HEX DUMP ===");
    LOGI("Total size: %zu bytes (including packet ID)", dataSize + 1);

    // Dump in rows of 16 bytes
    const uint8_t* data = bitStream.GetData();
    char hexLine[128];
    char asciiLine[32];
    for (size_t i = 0; i < dataSize; i += 16) {
        int hexPos = 0;
        int asciiPos = 0;
        for (size_t j = 0; j < 16 && i + j < dataSize; j++) {
            hexPos += sprintf(hexLine + hexPos, "%02x ", data[i + j]);
            asciiLine[asciiPos++] = (data[i + j] >= 32 && data[i + j] < 127) ? data[i + j] : '.';
        }
        asciiLine[asciiPos] = '\0';
        LOGI("  %04zx: %-48s %s", i, hexLine, asciiLine);
    }

    // Also log the parsed fields for verification
    LOGI("=== PARSED FIELDS ===");
    LOGI("  Netcode version: 0x%04x", MTA_DM_NETCODE_VERSION);
    LOGI("  MTA version: 0x%04x", MTA_DM_VERSION);
    LOGI("  Bitstream version: 0x%04x", MTA_DM_BITSTREAM_VERSION);
    LOGI("  Version string: %s", versionString.c_str());
    LOGI("  Game version: %d", m_playerInfo.gameVersion);
    LOGI("  Nickname: %s", m_playerInfo.nickname.c_str());

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(m_serverInfo.port);
    inet_pton(AF_INET, m_resolvedIP.c_str(), &serverAddr.sin_addr);

    ssize_t sent = sendto(m_socket, packet, dataSize + 1, 0,
                          reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr));

    LOGI("Sent JOIN_DATA packet (%zd bytes)", sent);
}

void CServerConnection::SendDisconnectPacket()
{
    // Send disconnect notification
    uint8_t packet[1] = {32};  // ID_DISCONNECTION_NOTIFICATION

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(m_serverInfo.port);
    inet_pton(AF_INET, m_resolvedIP.c_str(), &serverAddr.sin_addr);

    sendto(m_socket, packet, sizeof(packet), 0,
           reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr));
}

void CServerConnection::ComputeMD5Hash(const std::string& input, uint8_t output[MD5_HASH_LENGTH])
{
    MD5::Compute(input, output);
}

std::string CServerConnection::GenerateSerial()
{
    // Generate a pseudo-random serial for Android
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);

    std::stringstream ss;
    for (int i = 0; i < 32; i++)
    {
        ss << std::hex << dis(gen);
    }
    return ss.str();
}

//=============================================================================
// Connection Test Methods
//=============================================================================

std::string CServerConnection::TestDNSResolution(const std::string& hostname)
{
    auto startTime = GetCurrentTimeMs();

    // Check if already an IP address
    in_addr addr;
    if (inet_pton(AF_INET, hostname.c_str(), &addr) == 1)
    {
        m_testResults.dnsResolved = true;
        m_testResults.resolvedIP = hostname;
        m_testResults.dnsTimeMs = 0;
        return hostname;
    }

    // Resolve hostname
    addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    addrinfo* result = nullptr;
    int ret = getaddrinfo(hostname.c_str(), nullptr, &hints, &result);

    m_testResults.dnsTimeMs = static_cast<uint32_t>(GetCurrentTimeMs() - startTime);

    if (ret != 0 || result == nullptr)
    {
        m_testResults.dnsResolved = false;
        m_testResults.lastError = gai_strerror(ret);
        LOGE("DNS resolution failed: %s", m_testResults.lastError.c_str());
        return "";
    }

    char ipBuffer[INET_ADDRSTRLEN];
    sockaddr_in* sockaddr = reinterpret_cast<sockaddr_in*>(result->ai_addr);
    inet_ntop(AF_INET, &sockaddr->sin_addr, ipBuffer, sizeof(ipBuffer));

    freeaddrinfo(result);

    m_testResults.dnsResolved = true;
    m_testResults.resolvedIP = ipBuffer;

    return ipBuffer;
}

bool CServerConnection::TestConnectivity(const std::string& host, uint16_t port, uint32_t timeoutMs)
{
    LOGI("Testing connectivity to %s:%d", host.c_str(), port);

    auto startTime = GetCurrentTimeMs();

    // Resolve DNS first
    std::string ip = TestDNSResolution(host);
    if (ip.empty())
    {
        return false;
    }

    // Create UDP socket
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0)
    {
        m_testResults.lastError = "Failed to create socket";
        return false;
    }

    // Set non-blocking
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    // Server address
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr);

    // Send a simple ping packet (MTA query protocol)
    // Format: 'M' 'T' 'A' 'S' <type>
    uint8_t pingPacket[] = {'M', 'T', 'A', 'S', 'p'};  // 'p' = ping

    ssize_t sent = sendto(sock, pingPacket, sizeof(pingPacket), 0,
                          reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr));

    if (sent < 0)
    {
        m_testResults.lastError = "Failed to send ping";
        close(sock);
        return false;
    }

    // Wait for response
    struct pollfd pfd;
    pfd.fd = sock;
    pfd.events = POLLIN;

    int result = poll(&pfd, 1, timeoutMs);

    if (result > 0 && (pfd.revents & POLLIN))
    {
        uint8_t buffer[1024];
        sockaddr_in fromAddr;
        socklen_t fromLen = sizeof(fromAddr);

        ssize_t received = recvfrom(sock, buffer, sizeof(buffer), 0,
                                    reinterpret_cast<sockaddr*>(&fromAddr), &fromLen);

        if (received > 0)
        {
            m_testResults.udpReachable = true;
            m_testResults.serverResponded = true;
            m_testResults.pingMs = static_cast<uint32_t>(GetCurrentTimeMs() - startTime);
            m_testResults.serverResponse = std::string(reinterpret_cast<char*>(buffer),
                                                       std::min(received, ssize_t(64)));

            LOGI("Server responded in %dms", m_testResults.pingMs);
            close(sock);
            return true;
        }
    }

    m_testResults.udpReachable = true;  // We could send
    m_testResults.serverResponded = false;
    m_testResults.lastError = "No response from server";

    close(sock);
    return false;
}

std::string CServerConnection::GetConnectionTestResults() const
{
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"dns_resolved\": " << (m_testResults.dnsResolved ? "true" : "false") << ",\n";
    ss << "  \"resolved_ip\": \"" << m_testResults.resolvedIP << "\",\n";
    ss << "  \"dns_time_ms\": " << m_testResults.dnsTimeMs << ",\n";
    ss << "  \"udp_reachable\": " << (m_testResults.udpReachable ? "true" : "false") << ",\n";
    ss << "  \"server_responded\": " << (m_testResults.serverResponded ? "true" : "false") << ",\n";
    ss << "  \"ping_ms\": " << m_testResults.pingMs << ",\n";
    ss << "  \"last_error\": \"" << m_testResults.lastError << "\"\n";
    ss << "}";
    return ss.str();
}

//=============================================================================
// Player Sync
//=============================================================================

void CServerConnection::SendPlayerSync(float x, float y, float z, float rotation,
                                         float vx, float vy, float vz, bool onGround)
{
    if (m_state != ServerConnectionState::CONNECTED)
    {
        return;  // Not connected
    }

    if (!m_network)
    {
        return;  // No network layer
    }

    // Rate limiting is handled by the caller
    auto bitStream = m_network->AllocateBitStream();
    if (!bitStream)
    {
        return;
    }

    // Sync context
    uint8_t syncTimeContext = static_cast<uint8_t>(GetCurrentTimeMs() & 0xFF);
    bitStream->Write(syncTimeContext);

    // Controller state (derived from movement for now)
    SControllerState controller = BuildControllerFromVelocity(vx, vy, rotation);
    WriteFullKeysync(controller, *bitStream);

    // Player puresync flags (PC format)
    SPlayerPuresyncFlags flags;
    flags.isOnGround = onGround;
    flags.isInWater = false;
    flags.hasJetPack = false;
    flags.isDucked = false;
    flags.wearsGoggles = false;
    flags.hasContact = false;
    flags.isChoking = false;
    flags.akimboTargetUp = false;
    flags.isOnFire = false;
    flags.hasAWeapon = false;
    flags.syncingVelocity = (vx != 0.0f || vy != 0.0f || vz != 0.0f);
    flags.stealthAiming = false;
    flags.isReloadingWeapon = false;
    flags.animInterrupted = false;
    flags.hangingDuringClimb = false;
    flags.Write(*bitStream);

    // Position (PC compressed format)
    SPcPositionSync pos(false);
    pos.x = x;
    pos.y = y;
    pos.z = z;
    pos.Write(*bitStream);

    // Rotation (PC 16-bit)
    SPcPedRotationSync pedRot;
    pedRot.rotation = rotation;
    pedRot.Write(*bitStream);

    // Velocity (PC format)
    if (flags.syncingVelocity)
    {
        SPcVelocitySync vel;
        vel.x = vx;
        vel.y = vy;
        vel.z = vz;
        vel.Write(*bitStream);
    }

    // Health/armor (8-bit)
    bitStream->Write(static_cast<uint8_t>(100));
    bitStream->Write(static_cast<uint8_t>(0));

    // Camera rotation (12-bit)
    SPcCameraRotationSync camRot;
    camRot.rotation = rotation;
    camRot.Write(*bitStream);

    // Camera orientation block (placeholder)
    WriteCameraOrientationPlaceholder(*bitStream, rotation);

    // Send packet directly via our socket (not through CNetAndroid which has a separate socket)
    if (m_socket >= 0)
    {
        // Build packet: packet ID + bitstream data
        std::vector<uint8_t> packetData;
        packetData.push_back(static_cast<uint8_t>(PacketID::PLAYER_PURESYNC));
        packetData.insert(packetData.end(), bitStream->GetData(),
                          bitStream->GetData() + bitStream->GetBytesUsed());

        // Create server address
        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(m_serverInfo.port);
        inet_pton(AF_INET, m_resolvedIP.c_str(), &serverAddr.sin_addr);

        // Send directly
        ssize_t sent = sendto(m_socket, packetData.data(), packetData.size(), 0,
                              reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr));

        // Debug logging (every 100th packet or on error)
        static int syncCount = 0;
        static int errorCount = 0;
        if (sent < 0)
        {
            if (++errorCount <= 5)  // Only log first 5 errors
            {
                LOGE("PURESYNC send error: %s", strerror(errno));
            }
        }
        else if (++syncCount >= 100)
        {
            LOGD("Sent PURESYNC: pos=(%.1f,%.1f,%.1f) rot=%.2f (%zd bytes)",
                 x, y, z, rotation, sent);
            syncCount = 0;
        }
    }
}

} // namespace MTA::Android::Network
