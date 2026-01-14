/*
 * MTA:SA Android - Network Implementation
 *
 * CNetAndroid implementation using POSIX sockets and UDP
 *
 * Phase 7: Multiplayer Logic
 */

#include "CNetAndroid.h"

#include <android/log.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>
#include <chrono>
#include <algorithm>

#define LOG_TAG "MTA-Network"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

namespace MTA::Android::Network
{

//=============================================================================
// Helper Functions
//=============================================================================

static uint64_t GetCurrentTimeMs()
{
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

static void SetSocketNonBlocking(int socket)
{
    int flags = fcntl(socket, F_GETFL, 0);
    if (flags != -1)
    {
        fcntl(socket, F_SETFL, flags | O_NONBLOCK);
    }
}

//=============================================================================
// NetBitStream Implementation
//=============================================================================

NetBitStream::NetBitStream()
{
    m_data.reserve(256);
}

NetBitStream::NetBitStream(const uint8_t* data, size_t sizeInBytes)
{
    m_data.assign(data, data + sizeInBytes);
    m_writeOffsetBits = sizeInBytes * 8;
}

NetBitStream::~NetBitStream() = default;

void NetBitStream::Reset()
{
    m_data.clear();
    m_readOffsetBits = 0;
    m_writeOffsetBits = 0;
}

void NetBitStream::ResetReadPointer()
{
    m_readOffsetBits = 0;
}

void NetBitStream::EnsureCapacity(size_t bitsNeeded)
{
    size_t bytesNeeded = (m_writeOffsetBits + bitsNeeded + 7) / 8;
    if (bytesNeeded > m_data.size())
    {
        m_data.resize(std::max(bytesNeeded, m_data.size() * 2 + 16));
    }
}

void NetBitStream::Write(uint8_t value)
{
    EnsureCapacity(8);
    if ((m_writeOffsetBits % 8) == 0)
    {
        m_data[m_writeOffsetBits / 8] = value;
    }
    else
    {
        WriteBits(&value, 8);
        return;
    }
    m_writeOffsetBits += 8;
}

void NetBitStream::Write(int8_t value)
{
    Write(static_cast<uint8_t>(value));
}

void NetBitStream::Write(uint16_t value)
{
    Write(static_cast<uint8_t>(value & 0xFF));
    Write(static_cast<uint8_t>((value >> 8) & 0xFF));
}

void NetBitStream::Write(int16_t value)
{
    Write(static_cast<uint16_t>(value));
}

void NetBitStream::Write(uint32_t value)
{
    Write(static_cast<uint8_t>(value & 0xFF));
    Write(static_cast<uint8_t>((value >> 8) & 0xFF));
    Write(static_cast<uint8_t>((value >> 16) & 0xFF));
    Write(static_cast<uint8_t>((value >> 24) & 0xFF));
}

void NetBitStream::Write(int32_t value)
{
    Write(static_cast<uint32_t>(value));
}

void NetBitStream::Write(float value)
{
    uint32_t temp;
    std::memcpy(&temp, &value, sizeof(float));
    Write(temp);
}

void NetBitStream::Write(double value)
{
    uint64_t temp;
    std::memcpy(&temp, &value, sizeof(double));
    Write(static_cast<uint32_t>(temp & 0xFFFFFFFF));
    Write(static_cast<uint32_t>((temp >> 32) & 0xFFFFFFFF));
}

void NetBitStream::Write(const char* data, size_t length)
{
    for (size_t i = 0; i < length; ++i)
    {
        Write(static_cast<uint8_t>(data[i]));
    }
}

void NetBitStream::Write(const std::string& str)
{
    Write(static_cast<uint16_t>(str.length()));
    Write(str.data(), str.length());
}

void NetBitStream::WriteCompressed(uint16_t value)
{
    if ((value & 0xFF00) == 0)
    {
        WriteBit(true);
        Write(static_cast<uint8_t>(value & 0xFF));
    }
    else
    {
        WriteBit(false);
        Write(value);
    }
}

void NetBitStream::WriteCompressed(int16_t value)
{
    WriteCompressed(static_cast<uint16_t>(value));
}

void NetBitStream::WriteCompressed(uint32_t value)
{
    if ((value & 0xFFFF0000) == 0)
    {
        WriteBit(true);
        WriteCompressed(static_cast<uint16_t>(value & 0xFFFF));
    }
    else
    {
        WriteBit(false);
        Write(value);
    }
}

void NetBitStream::WriteCompressed(int32_t value)
{
    WriteCompressed(static_cast<uint32_t>(value));
}

void NetBitStream::WriteBits(const uint8_t* data, size_t numBits)
{
    EnsureCapacity(numBits);

    for (size_t i = 0; i < numBits; ++i)
    {
        size_t byteIndex = i / 8;
        size_t bitIndex = i % 8;
        bool bit = (data[byteIndex] >> bitIndex) & 1;
        WriteBit(bit);
    }
}

void NetBitStream::WriteBit(bool value)
{
    EnsureCapacity(1);

    size_t byteOffset = m_writeOffsetBits / 8;
    size_t bitOffset = m_writeOffsetBits % 8;

    if (bitOffset == 0)
    {
        m_data[byteOffset] = 0;
    }

    if (value)
    {
        m_data[byteOffset] |= (1 << bitOffset);
    }

    m_writeOffsetBits++;
}

void NetBitStream::WriteNormVector(float x, float y, float z)
{
    Write(x);
    Write(y);
    Write(z);
}

void NetBitStream::WriteVector(float x, float y, float z)
{
    Write(x);
    Write(y);
    Write(z);
}

void NetBitStream::WriteNormQuat(float w, float x, float y, float z)
{
    Write(w);
    Write(x);
    Write(y);
    Write(z);
}

bool NetBitStream::Read(uint8_t& value)
{
    if (m_readOffsetBits + 8 > m_writeOffsetBits) return false;

    if ((m_readOffsetBits % 8) == 0)
    {
        value = m_data[m_readOffsetBits / 8];
        m_readOffsetBits += 8;
        return true;
    }
    else
    {
        return ReadBits(&value, 8);
    }
}

bool NetBitStream::Read(int8_t& value)
{
    uint8_t temp;
    if (!Read(temp)) return false;
    value = static_cast<int8_t>(temp);
    return true;
}

bool NetBitStream::Read(uint16_t& value)
{
    uint8_t low, high;
    if (!Read(low)) return false;
    if (!Read(high)) return false;
    value = low | (static_cast<uint16_t>(high) << 8);
    return true;
}

bool NetBitStream::Read(int16_t& value)
{
    uint16_t temp;
    if (!Read(temp)) return false;
    value = static_cast<int16_t>(temp);
    return true;
}

bool NetBitStream::Read(uint32_t& value)
{
    uint8_t b0, b1, b2, b3;
    if (!Read(b0)) return false;
    if (!Read(b1)) return false;
    if (!Read(b2)) return false;
    if (!Read(b3)) return false;
    value = b0 | (static_cast<uint32_t>(b1) << 8) |
            (static_cast<uint32_t>(b2) << 16) | (static_cast<uint32_t>(b3) << 24);
    return true;
}

bool NetBitStream::Read(int32_t& value)
{
    uint32_t temp;
    if (!Read(temp)) return false;
    value = static_cast<int32_t>(temp);
    return true;
}

bool NetBitStream::Read(float& value)
{
    uint32_t temp;
    if (!Read(temp)) return false;
    std::memcpy(&value, &temp, sizeof(float));
    return true;
}

bool NetBitStream::Read(double& value)
{
    uint32_t low, high;
    if (!Read(low)) return false;
    if (!Read(high)) return false;
    uint64_t temp = low | (static_cast<uint64_t>(high) << 32);
    std::memcpy(&value, &temp, sizeof(double));
    return true;
}

bool NetBitStream::Read(char* data, size_t length)
{
    for (size_t i = 0; i < length; ++i)
    {
        uint8_t byte;
        if (!Read(byte)) return false;
        data[i] = static_cast<char>(byte);
    }
    return true;
}

bool NetBitStream::Read(std::string& str, size_t maxLength)
{
    uint16_t length;
    if (!Read(length)) return false;
    if (length > maxLength) return false;

    str.resize(length);
    return Read(&str[0], length);
}

bool NetBitStream::ReadCompressed(uint16_t& value)
{
    bool isByte = ReadBit();
    if (isByte)
    {
        uint8_t byte = 0;
        if (!Read(byte)) return false;
        value = byte;
        return true;
    }

    return Read(value);
}

bool NetBitStream::ReadCompressed(int16_t& value)
{
    uint16_t temp;
    if (!ReadCompressed(temp)) return false;
    value = static_cast<int16_t>(temp);
    return true;
}

bool NetBitStream::ReadCompressed(uint32_t& value)
{
    bool isShort = ReadBit();
    if (isShort)
    {
        uint16_t temp = 0;
        if (!ReadCompressed(temp)) return false;
        value = temp;
        return true;
    }

    return Read(value);
}

bool NetBitStream::ReadCompressed(int32_t& value)
{
    uint32_t temp;
    if (!ReadCompressed(temp)) return false;
    value = static_cast<int32_t>(temp);
    return true;
}

bool NetBitStream::ReadBits(uint8_t* data, size_t numBits)
{
    if (m_readOffsetBits + numBits > m_writeOffsetBits) return false;

    std::memset(data, 0, (numBits + 7) / 8);

    for (size_t i = 0; i < numBits; ++i)
    {
        size_t srcByteIndex = m_readOffsetBits / 8;
        size_t srcBitIndex = m_readOffsetBits % 8;
        bool bit = (m_data[srcByteIndex] >> srcBitIndex) & 1;

        if (bit)
        {
            size_t dstByteIndex = i / 8;
            size_t dstBitIndex = i % 8;
            data[dstByteIndex] |= (1 << dstBitIndex);
        }

        m_readOffsetBits++;
    }
    return true;
}

bool NetBitStream::ReadBit()
{
    if (m_readOffsetBits >= m_writeOffsetBits) return false;

    size_t byteOffset = m_readOffsetBits / 8;
    size_t bitOffset = m_readOffsetBits % 8;
    bool value = (m_data[byteOffset] >> bitOffset) & 1;
    m_readOffsetBits++;
    return value;
}

bool NetBitStream::ReadNormVector(float& x, float& y, float& z)
{
    return Read(x) && Read(y) && Read(z);
}

bool NetBitStream::ReadVector(float& x, float& y, float& z)
{
    if (!Read(x)) return false;
    if (!Read(y)) return false;
    if (!Read(z)) return false;
    return true;
}

bool NetBitStream::ReadNormQuat(float& w, float& x, float& y, float& z)
{
    return Read(w) && Read(x) && Read(y) && Read(z);
}

int NetBitStream::GetReadOffsetBits() const
{
    return static_cast<int>(m_readOffsetBits);
}

void NetBitStream::SetReadOffsetBits(int offset)
{
    m_readOffsetBits = static_cast<size_t>(offset);
}

int NetBitStream::GetWriteOffsetBits() const
{
    return static_cast<int>(m_writeOffsetBits);
}

int NetBitStream::GetBitsUsed() const
{
    return static_cast<int>(m_writeOffsetBits);
}

int NetBitStream::GetBytesUsed() const
{
    return static_cast<int>((m_writeOffsetBits + 7) / 8);
}

int NetBitStream::GetUnreadBits() const
{
    return static_cast<int>(m_writeOffsetBits - m_readOffsetBits);
}

void NetBitStream::AlignWriteToByte()
{
    if (m_writeOffsetBits % 8 != 0)
    {
        m_writeOffsetBits = ((m_writeOffsetBits / 8) + 1) * 8;
    }
}

void NetBitStream::AlignReadToByte()
{
    if (m_readOffsetBits % 8 != 0)
    {
        m_readOffsetBits = ((m_readOffsetBits / 8) + 1) * 8;
    }
}

const uint8_t* NetBitStream::GetData() const
{
    return m_data.data();
}

uint8_t* NetBitStream::GetData()
{
    return m_data.data();
}

bool NetBitStream::CanReadBytes(size_t numBytes) const
{
    return (m_readOffsetBits + numBytes * 8) <= m_writeOffsetBits;
}

//=============================================================================
// CNetAndroid Implementation
//=============================================================================

CNetAndroid::CNetAndroid() = default;

CNetAndroid::~CNetAndroid()
{
    Shutdown();
}

bool CNetAndroid::Initialize()
{
    if (m_initialized)
    {
        return true;
    }

    LOGI("CNetAndroid: Initializing network subsystem");

    // Reset state
    m_state = ConnectionState::Disconnected;
    m_connectionError = 0;
    m_ping = 0;
    std::memset(m_packetStats, 0, sizeof(m_packetStats));
    m_stats = NetStatistics{};

    m_initialized = true;
    LOGI("CNetAndroid: Network subsystem initialized");

    return true;
}

void CNetAndroid::Shutdown()
{
    if (!m_initialized)
    {
        return;
    }

    LOGI("CNetAndroid: Shutting down network subsystem");

    Disconnect("Shutdown");

    // Wait for network thread
    m_running = false;
    if (m_networkThread.joinable())
    {
        m_networkThread.join();
    }

    m_initialized = false;
    LOGI("CNetAndroid: Network subsystem shutdown complete");
}

bool CNetAndroid::Connect(const std::string& host, uint16_t port)
{
    if (!m_initialized)
    {
        LOGE("CNetAndroid: Cannot connect - not initialized");
        return false;
    }

    if (m_state != ConnectionState::Disconnected)
    {
        LOGW("CNetAndroid: Already connected or connecting");
        return false;
    }

    LOGI("CNetAndroid: Connecting to %s:%d", host.c_str(), port);

    // Resolve hostname
    if (!ResolveHost(host, m_serverIP))
    {
        LOGE("CNetAndroid: Failed to resolve hostname: %s", host.c_str());
        m_connectionError = 1;  // DNS resolution failed
        return false;
    }

    m_serverHost = host;
    m_serverPort = port;
    m_state = ConnectionState::Connecting;
    m_connectionStartTime = GetCurrentTimeMs();

    // Create UDP socket
    m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_socket < 0)
    {
        LOGE("CNetAndroid: Failed to create socket: %s", strerror(errno));
        m_state = ConnectionState::Failed;
        m_connectionError = 2;  // Socket creation failed
        return false;
    }

    // Set non-blocking
    SetSocketNonBlocking(m_socket);

    // Bind to client port
    struct sockaddr_in localAddr{};
    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = INADDR_ANY;
    localAddr.sin_port = htons(m_clientPort);

    if (bind(m_socket, reinterpret_cast<struct sockaddr*>(&localAddr), sizeof(localAddr)) < 0)
    {
        LOGE("CNetAndroid: Failed to bind socket: %s", strerror(errno));
        close(m_socket);
        m_socket = -1;
        m_state = ConnectionState::Failed;
        m_connectionError = 3;  // Bind failed
        return false;
    }

    // Start network thread
    m_running = true;
    m_networkThread = std::thread(&CNetAndroid::NetworkThread, this);

    LOGI("CNetAndroid: Connection initiated to %s:%d", m_serverIP.c_str(), port);

    return true;
}

void CNetAndroid::Disconnect(const std::string& reason)
{
    if (m_state == ConnectionState::Disconnected)
    {
        return;
    }

    LOGI("CNetAndroid: Disconnecting: %s", reason.c_str());

    m_state = ConnectionState::Disconnecting;

    // Stop network thread
    m_running = false;
    if (m_networkThread.joinable())
    {
        m_networkThread.join();
    }

    // Close socket
    if (m_socket >= 0)
    {
        close(m_socket);
        m_socket = -1;
    }

    // Clear queues
    {
        std::lock_guard<std::mutex> lock(m_incomingMutex);
        while (!m_incomingPackets.empty()) m_incomingPackets.pop();
    }
    {
        std::lock_guard<std::mutex> lock(m_outgoingMutex);
        while (!m_outgoingPackets.empty()) m_outgoingPackets.pop();
    }

    m_state = ConnectionState::Disconnected;
    LOGI("CNetAndroid: Disconnected");
}

std::string CNetAndroid::GetConnectedServer(bool includePort) const
{
    if (m_state != ConnectionState::Connected)
    {
        return "";
    }

    if (includePort)
    {
        return m_serverHost + ":" + std::to_string(m_serverPort);
    }
    return m_serverHost;
}

std::unique_ptr<NetBitStream> CNetAndroid::AllocateBitStream()
{
    auto bitStream = std::make_unique<NetBitStream>();
    bitStream->SetVersion(m_serverBitStreamVersion);
    return bitStream;
}

bool CNetAndroid::SendPacket(PacketID packetId, NetBitStream& bitStream,
                              uint8_t priority, uint8_t reliability,
                              uint8_t orderingChannel)
{
    if (m_state != ConnectionState::Connected && m_state != ConnectionState::Connecting)
    {
        return false;
    }

    // Build packet data
    std::vector<uint8_t> packetData;
    packetData.push_back(static_cast<uint8_t>(packetId));
    packetData.insert(packetData.end(), bitStream.GetData(),
                      bitStream.GetData() + bitStream.GetBytesUsed());

    // Queue for sending
    {
        std::lock_guard<std::mutex> lock(m_outgoingMutex);
        m_outgoingPackets.push({packetId, std::move(packetData)});
    }

    // Update stats
    m_stats.packetsSent++;
    m_stats.bytesSent += bitStream.GetBytesUsed() + 1;
    m_packetStats[static_cast<uint8_t>(packetId)].count++;
    m_packetStats[static_cast<uint8_t>(packetId)].totalBytes += bitStream.GetBytesUsed() + 1;

    return true;
}

void CNetAndroid::RegisterPacketHandler(PacketHandler handler)
{
    m_packetHandler = std::move(handler);
}

void CNetAndroid::DoPulse()
{
    if (!m_initialized)
    {
        return;
    }

    // Process incoming packets
    ProcessReceivedPackets();

    // Check for timeout
    if (m_state == ConnectionState::Connecting)
    {
        uint64_t elapsed = GetCurrentTimeMs() - m_connectionStartTime;
        if (elapsed > Protocol::CONNECT_TIMEOUT_MS)
        {
            LOGE("CNetAndroid: Connection timeout");
            m_connectionError = 4;  // Connection timeout
            Disconnect("Connection timeout");
            m_state = ConnectionState::Failed;
        }
    }
    else if (m_state == ConnectionState::Connected)
    {
        uint64_t elapsed = GetCurrentTimeMs() - m_lastPacketTime;
        if (elapsed > m_timeoutMs)
        {
            LOGE("CNetAndroid: Connection lost (timeout)");
            Disconnect("Connection lost");
        }
    }

    // Update statistics
    UpdateStatistics();
}

uint32_t CNetAndroid::GetTime() const
{
    return static_cast<uint32_t>(GetCurrentTimeMs() - m_connectionStartTime);
}

std::string CNetAndroid::GetLocalIP() const
{
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) != 0)
    {
        return "0.0.0.0";
    }

    struct hostent* host = gethostbyname(hostname);
    if (host == nullptr || host->h_addr_list[0] == nullptr)
    {
        return "0.0.0.0";
    }

    return inet_ntoa(*reinterpret_cast<struct in_addr*>(host->h_addr_list[0]));
}

std::string CNetAndroid::GetSerial() const
{
    // Generate a pseudo-serial based on device info
    // TODO: Implement proper serial generation
    return "ANDROID-MTA-0000000000000000";
}

std::string CNetAndroid::GetConnectionErrorString() const
{
    switch (m_connectionError)
    {
        case 0: return "No error";
        case 1: return "DNS resolution failed";
        case 2: return "Socket creation failed";
        case 3: return "Socket bind failed";
        case 4: return "Connection timeout";
        case 5: return "Connection refused";
        case 6: return "Server full";
        case 7: return "Invalid password";
        case 8: return "Banned";
        default: return "Unknown error";
    }
}

void CNetAndroid::Reset()
{
    Disconnect("Reset");
    m_connectionError = 0;
    std::memset(m_packetStats, 0, sizeof(m_packetStats));
    m_stats = NetStatistics{};
}

void CNetAndroid::NetworkThread()
{
    LOGI("CNetAndroid: Network thread started");

    struct sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(m_serverPort);
    inet_pton(AF_INET, m_serverIP.c_str(), &serverAddr.sin_addr);

    uint8_t recvBuffer[4096];

    while (m_running)
    {
        // Send queued packets
        {
            std::lock_guard<std::mutex> lock(m_outgoingMutex);
            while (!m_outgoingPackets.empty())
            {
                auto& packet = m_outgoingPackets.front();
                ssize_t sent = sendto(m_socket, packet.second.data(), packet.second.size(), 0,
                                      reinterpret_cast<struct sockaddr*>(&serverAddr), sizeof(serverAddr));
                if (sent < 0)
                {
                    if (errno != EAGAIN && errno != EWOULDBLOCK)
                    {
                        LOGE("CNetAndroid: Send error: %s", strerror(errno));
                    }
                }
                m_outgoingPackets.pop();
            }
        }

        // Receive packets
        struct sockaddr_in fromAddr{};
        socklen_t fromLen = sizeof(fromAddr);
        ssize_t received = recvfrom(m_socket, recvBuffer, sizeof(recvBuffer), 0,
                                    reinterpret_cast<struct sockaddr*>(&fromAddr), &fromLen);

        if (received > 0)
        {
            m_lastPacketTime = GetCurrentTimeMs();

            // Update connection state if we were connecting
            if (m_state == ConnectionState::Connecting)
            {
                m_state = ConnectionState::Connected;
                LOGI("CNetAndroid: Connected to server");
            }

            // Queue received packet
            Packet packet;
            packet.id = static_cast<PacketID>(recvBuffer[0]);
            packet.data.assign(recvBuffer + 1, recvBuffer + received);
            packet.timestamp = static_cast<uint32_t>(GetCurrentTimeMs());
            packet.senderAddress = inet_ntoa(fromAddr.sin_addr);
            packet.senderPort = ntohs(fromAddr.sin_port);

            {
                std::lock_guard<std::mutex> lock(m_incomingMutex);
                m_incomingPackets.push(std::move(packet));
            }

            // Update stats
            m_stats.packetsReceived++;
            m_stats.bytesReceived += received;
            m_packetStats[recvBuffer[0]].count++;
            m_packetStats[recvBuffer[0]].totalBytes += received;
        }
        else if (received < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        {
            LOGE("CNetAndroid: Receive error: %s", strerror(errno));
        }

        // Small sleep to prevent busy loop
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    LOGI("CNetAndroid: Network thread stopped");
}

void CNetAndroid::ProcessReceivedPackets()
{
    std::queue<Packet> packetsToProcess;

    {
        std::lock_guard<std::mutex> lock(m_incomingMutex);
        std::swap(packetsToProcess, m_incomingPackets);
    }

    while (!packetsToProcess.empty())
    {
        Packet& packet = packetsToProcess.front();

        if (m_packetHandler)
        {
            NetBitStream bitStream(packet.data.data(), packet.data.size());
            bitStream.SetVersion(m_serverBitStreamVersion);

            m_packetHandler(packet.id, bitStream);
        }
        else
        {
            LOGD("CNetAndroid: Received packet ID %d, no handler", static_cast<int>(packet.id));
        }

        packetsToProcess.pop();
    }
}

void CNetAndroid::UpdateStatistics()
{
    // Calculate bandwidth (simplified)
    static uint64_t lastUpdateTime = 0;
    static uint64_t lastBytesSent = 0;
    static uint64_t lastBytesReceived = 0;

    uint64_t now = GetCurrentTimeMs();
    uint64_t elapsed = now - lastUpdateTime;

    if (elapsed >= 1000)  // Update every second
    {
        uint64_t sentDelta = m_stats.bytesSent - lastBytesSent;
        uint64_t recvDelta = m_stats.bytesReceived - lastBytesReceived;

        m_stats.sendBandwidth = static_cast<float>(sentDelta * 1000 / elapsed);
        m_stats.receiveBandwidth = static_cast<float>(recvDelta * 1000 / elapsed);

        lastUpdateTime = now;
        lastBytesSent = m_stats.bytesSent;
        lastBytesReceived = m_stats.bytesReceived;
    }
}

bool CNetAndroid::ResolveHost(const std::string& host, std::string& outIP)
{
    // Check if already an IP address
    struct in_addr addr;
    if (inet_pton(AF_INET, host.c_str(), &addr) == 1)
    {
        outIP = host;
        return true;
    }

    // Resolve hostname
    struct hostent* he = gethostbyname(host.c_str());
    if (he == nullptr)
    {
        return false;
    }

    outIP = inet_ntoa(*reinterpret_cast<struct in_addr*>(he->h_addr_list[0]));
    return true;
}

} // namespace MTA::Android::Network
