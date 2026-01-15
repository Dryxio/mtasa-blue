/*****************************************************************************
 *
 *  PROJECT:     MTA:SA Server - Android Network Module
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        Server/net-android/CNetServerAndroid.cpp
 *  PURPOSE:     Network server for Android clients (no Anti-Cheat)
 *
 *****************************************************************************/

#include "CNetServerAndroid.h"
#include "CNetBitStreamAndroid.h"

#include <cstring>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <algorithm>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <poll.h>
    #include <fcntl.h>
    #include <errno.h>
    #define closesocket close
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
#endif

// Logging with fflush for immediate output
#ifdef _WIN32
    #define NET_LOG(fmt, ...) do { printf("[net-android] " fmt "\n", ##__VA_ARGS__); fflush(stdout); } while(0)
#else
    #define NET_LOG(fmt, ...) do { printf("[net-android] " fmt "\n", ##__VA_ARGS__); fflush(stdout); } while(0)
#endif

//=============================================================================
// Constructor / Destructor
//=============================================================================

CNetServerAndroid::CNetServerAndroid()
{
    // Initialize random seed for GUID generation
    srand(static_cast<unsigned>(time(nullptr)));
    m_serverGUID = ((uint64_t)rand() << 32) | rand();

    // Clear packet stats
    memset(m_packetStats, 0, sizeof(m_packetStats));

    NET_LOG("CNetServerAndroid created, server GUID: %llx",
            (unsigned long long)m_serverGUID);
}

CNetServerAndroid::~CNetServerAndroid()
{
    StopNetwork();
}

//=============================================================================
// Core Network Operations
//=============================================================================

bool CNetServerAndroid::StartNetwork(const char* szIP, unsigned short usServerPort,
                                      unsigned int uiAllowedPlayers, const char* szServerName)
{
    NET_LOG("Starting network on %s:%d (max %d players)",
            szIP ? szIP : "0.0.0.0", usServerPort, uiAllowedPlayers);

#ifdef _WIN32
    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        NET_LOG("WSAStartup failed");
        return false;
    }
#endif

    // Create UDP socket
    m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_socket == INVALID_SOCKET)
    {
        NET_LOG("Failed to create socket: %s", strerror(errno));
        return false;
    }

    // Set socket options
    int optval = 1;
    setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&optval, sizeof(optval));

    // Set non-blocking
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(m_socket, FIONBIO, &mode);
#else
    int flags = fcntl(m_socket, F_GETFL, 0);
    fcntl(m_socket, F_SETFL, flags | O_NONBLOCK);
#endif

    // Bind socket
    sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(usServerPort);

    if (szIP && szIP[0] != '\0')
    {
        serverAddr.sin_addr.s_addr = inet_addr(szIP);
        m_strServerIP = szIP;
    }
    else
    {
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        m_strServerIP = "0.0.0.0";
    }

    if (bind(m_socket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        NET_LOG("Failed to bind socket: %s", strerror(errno));
        closesocket(m_socket);
        m_socket = -1;
        return false;
    }

    m_usPort = usServerPort;
    m_uiMaxPlayers = uiAllowedPlayers;
    m_strServerName = szServerName ? szServerName : "MTA Android Server";
    m_startTime = GetTimeMs();

    // Start network thread
    m_bRunning = true;
    m_networkThread = std::thread(&CNetServerAndroid::NetworkThreadFunc, this);

    NET_LOG("Network started successfully on port %d", usServerPort);
    NET_LOG("Waiting for Android client connections (MTA RakNet 3.x protocol)...");

    return true;
}

void CNetServerAndroid::StopNetwork()
{
    if (!m_bRunning)
        return;

    NET_LOG("Stopping network...");

    // Stop network thread
    m_bRunning = false;
    if (m_networkThread.joinable())
    {
        m_networkThread.join();
    }

    // Close socket
    if (m_socket != -1)
    {
        closesocket(m_socket);
        m_socket = -1;
    }

    // Clear clients
    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        m_clients.clear();
    }

#ifdef _WIN32
    WSACleanup();
#endif

    NET_LOG("Network stopped");
}

void CNetServerAndroid::DoPulse()
{
    // Debug: log first call to verify DoPulse is even being called
    static bool firstCall = true;
    if (firstCall)
    {
        NET_LOG("DoPulse: FIRST CALL - packet queue processing enabled");
        firstCall = false;
    }

    // CRITICAL: Process queued packets from network thread
    // This runs in the MAIN THREAD, which is safe for deathmatch.so
    ProcessQueuedPackets();

    // Check for timed-out clients
    uint64_t now = GetTimeMs();

    // Debug: log occasionally to verify DoPulse is being called
    static int pulseCount = 0;
    static uint64_t lastLogTime = 0;
    pulseCount++;

    if (now - lastLogTime > 10000)  // Log every 10 seconds
    {
        size_t clientCount;
        {
            std::lock_guard<std::mutex> lock(m_clientsMutex);
            clientCount = m_clients.size();
        }
        NET_LOG("DoPulse: %d calls, %zu clients, queue processing active", pulseCount, clientCount);
        lastLogTime = now;
    }

    // Collect timed-out clients with their info (need to copy data before releasing lock)
    struct TimedOutClient {
        NetServerPlayerID playerID;
        uint16_t bitstreamVersion;
    };
    std::vector<TimedOutClient> timedOutClients;

    // First pass: collect timed-out clients while holding lock
    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);

        for (auto& pair : m_clients)
        {
            auto& client = pair.second;
            if (client.state == ClientState::CONNECTED)
            {
                // Timeout after 30 seconds of no packets
                if (now - client.lastPacketTime > 30000)
                {
                    TimedOutClient toc;
                    toc.playerID = client.playerID;
                    toc.bitstreamVersion = client.bitstreamVersion;
                    timedOutClients.push_back(toc);
                }
            }
        }

        // Remove clients from map while still holding lock
        for (const auto& toc : timedOutClients)
        {
            uint64_t key = ((uint64_t)toc.playerID.GetBinaryAddress() << 16) | toc.playerID.GetPort();
            m_clients.erase(key);
        }
    }
    // Lock released here

    // Notify packet handler WITHOUT holding the lock
    // This prevents deadlock if game module calls back into our code
    for (const auto& toc : timedOutClients)
    {
        in_addr addr;
        addr.s_addr = htonl(toc.playerID.GetBinaryAddress());
        NET_LOG("Client %s:%d timed out", inet_ntoa(addr), toc.playerID.GetPort());

        // Note: We intentionally don't call m_pfnPacketHandler here
        // The game module (deathmatch.so) may not handle CONNECTION_LOST correctly
        // and crashes when we try to notify it. For now, just silently remove.
        // TODO: Investigate proper disconnect notification for deathmatch.so
        NET_LOG("   Client removed (timeout)");
    }
}

void CNetServerAndroid::CheckClientTimeouts(uint64_t now, uint64_t timeoutMs)
{
    // Collect timed-out clients with their info
    struct TimedOutClient {
        NetServerPlayerID playerID;
        uint16_t bitstreamVersion;
    };
    std::vector<TimedOutClient> timedOutClients;

    // First pass: collect timed-out clients while holding lock
    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);

        for (auto& pair : m_clients)
        {
            auto& client = pair.second;
            if (client.state == ClientState::CONNECTED)
            {
                if (now - client.lastPacketTime > timeoutMs)
                {
                    TimedOutClient toc;
                    toc.playerID = client.playerID;
                    toc.bitstreamVersion = client.bitstreamVersion;
                    timedOutClients.push_back(toc);

                    in_addr addr;
                    addr.s_addr = htonl(client.playerID.GetBinaryAddress());
                    NET_LOG("Client %s:%d timed out (no packets for %lu ms)",
                            inet_ntoa(addr), client.playerID.GetPort(),
                            (unsigned long)(now - client.lastPacketTime));
                }
            }
        }

        // Remove clients from map while still holding lock
        for (const auto& toc : timedOutClients)
        {
            uint64_t key = ((uint64_t)toc.playerID.GetBinaryAddress() << 16) | toc.playerID.GetPort();
            m_clients.erase(key);
            NET_LOG("   Client removed from map");
        }
    }
    // Lock released here - don't notify game module, it causes crashes
}

//=============================================================================
// Network Thread
//=============================================================================

void CNetServerAndroid::NetworkThreadFunc()
{
    NET_LOG("Network thread started");

    uint8_t buffer[4096];
    uint64_t lastTimeoutCheck = GetTimeMs();
    constexpr uint64_t TIMEOUT_CHECK_INTERVAL = 5000;  // Check every 5 seconds
    constexpr uint64_t CLIENT_TIMEOUT = 30000;         // 30 second timeout

    while (m_bRunning)
    {
        // Poll for incoming packets
        struct pollfd pfd;
        pfd.fd = m_socket;
        pfd.events = POLLIN;

#ifdef _WIN32
        int result = WSAPoll(&pfd, 1, 10);
#else
        int result = poll(&pfd, 1, 10);
#endif

        if (result > 0 && (pfd.revents & POLLIN))
        {
            sockaddr_in fromAddr;
            socklen_t fromLen = sizeof(fromAddr);

            ssize_t received = recvfrom(m_socket, (char*)buffer, sizeof(buffer), 0,
                                        (sockaddr*)&fromAddr, &fromLen);

            if (received > 0)
            {
                ProcessIncomingPacket(buffer, (int)received, fromAddr);
            }
        }

        // Periodically check for timed-out clients (since DoPulse isn't being called)
        uint64_t now = GetTimeMs();
        if (now - lastTimeoutCheck >= TIMEOUT_CHECK_INTERVAL)
        {
            lastTimeoutCheck = now;
            CheckClientTimeouts(now, CLIENT_TIMEOUT);
        }
    }

    NET_LOG("Network thread stopped");
}

//=============================================================================
// Packet Processing
//=============================================================================

void CNetServerAndroid::ProcessIncomingPacket(const uint8_t* data, int length,
                                               const sockaddr_in& fromAddr)
{
    if (length < 1)
        return;

    uint8_t packetID = data[0];
    LogPacket("<-", data, length, fromAddr);

    // Track latest port for this IP (to handle NAT port changes)
    {
        std::lock_guard<std::mutex> portLock(m_portMapMutex);
        uint32_t ip = ntohl(fromAddr.sin_addr.s_addr);
        uint16_t port = ntohs(fromAddr.sin_port);
        m_ipToLatestPort[ip] = port;
    }

    // Get or create client - hold lock for entire packet processing
    // to prevent DoPulse from removing the client while we're using it
    std::unique_lock<std::mutex> lock(m_clientsMutex);

    NetServerPlayerID playerID = MakePlayerID(fromAddr);
    uint64_t key = ((uint64_t)playerID.GetBinaryAddress() << 16) | playerID.GetPort();

    auto it = m_clients.find(key);
    if (it == m_clients.end())
    {
        // Create new client
        ClientConnection newClient;
        newClient.playerID = playerID;
        newClient.state = ClientState::DISCONNECTED;
        newClient.connectTime = GetTimeMs();
        newClient.lastPacketTime = GetTimeMs();
        m_clients[key] = newClient;
        it = m_clients.find(key);
    }

    ClientConnection* client = &it->second;

    client->lastPacketTime = GetTimeMs();
    client->packetsReceived++;
    client->bytesReceived += length;

    // Log client state for debugging
    NET_LOG("   Client state: %d, packet ID: 0x%02X",
            static_cast<int>(client->state), packetID);

    // IMPORTANT: Packet ID 0x01 is both PING and PLAYER_JOINDATA
    // Check client state to determine which one it is
    if (packetID == 0x01 && client->state == ClientState::AWAITING_JOINDATA)
    {
        // This is PLAYER_JOINDATA, not a PING
        NET_LOG("   -> Handling as PLAYER_JOINDATA (state=AWAITING_JOINDATA)");
        HandlePlayerJoinData(data, length, *client);

        // Copy data we need before releasing lock
        NetServerPlayerID copyPlayerID = client->playerID;
        uint16_t copyBitstreamVersion = client->bitstreamVersion;

        // Release lock
        lock.unlock();

        // QUEUE packet for main thread processing (instead of calling handler directly)
        // This avoids the deadlock with CSimPlayerManager::m_CS mutex
        NET_LOG("*** QUEUING PLAYER_JOINDATA for main thread ***");
        QueuePacketForMainThread(MTAPacketID::PLAYER_JOINDATA, copyPlayerID,
                                  data + 1, length - 1, copyBitstreamVersion);
        return;
    }

    // Handle MTA query packets (starts with "MTA")
    if (length >= 4 && data[0] == 'M' && data[1] == 'T' && data[2] == 'A')
    {
        // MTA query packet (e.g., "MTAS" for status, "MTAC" for client)
        NET_LOG("   -> MTA query packet: %c%c%c%c",
                data[0], data[1], data[2], data[3]);

        // If it's a client connection check, acknowledge it
        if (data[3] == 'C' && client->state == ClientState::AWAITING_JOINDATA)
        {
            NET_LOG("   -> Ignoring MTAC packet (already in AWAITING_JOINDATA)");
        }
        else if (data[3] == 'S' && data[4] == 'p')
        {
            // Status ping - respond with basic info
            NET_LOG("   -> MTA status ping");
        }
        return;
    }

    // Handle based on packet ID
    switch (packetID)
    {
        case RakNetPacketID::OPEN_CONNECTION_REQUEST:
            HandleOpenConnectionRequest(data, length, fromAddr, *client);
            break;

        case RakNetPacketID::CONNECTION_REQUEST:
            // Handle connection request - this sends CONNECTION_REQUEST_ACCEPTED and MOD_NAME
            HandleConnectionRequest(data, length, *client);
            break;

        case RakNetPacketID::INTERNAL_PING:
        {
            // True ping (only if not PLAYER_JOINDATA context)
            NET_LOG("   -> INTERNAL_PING");
            uint8_t pong[9];
            pong[0] = RakNetPacketID::CONNECTED_PONG;
            if (length >= 9)
            {
                memcpy(pong + 1, data + 1, 8);
            }
            else
            {
                memset(pong + 1, 0, 8);
            }
            SendRawPacket(pong, sizeof(pong), fromAddr);
            break;
        }

        case RakNetPacketID::PING:
        {
            // Note: packet ID 0x01 might be PLAYER_JOINDATA (handled above)
            // If we reach here, it's a true ping
            NET_LOG("   -> PING (not PLAYER_JOINDATA)");
            uint8_t pong[9];
            pong[0] = RakNetPacketID::CONNECTED_PONG;
            if (length >= 9)
            {
                memcpy(pong + 1, data + 1, 8);
            }
            else
            {
                memset(pong + 1, 0, 8);
            }
            SendRawPacket(pong, sizeof(pong), fromAddr);
            break;
        }

        case RakNetPacketID::NEW_INCOMING_CONNECTION:
        {
            // Client confirmed connection
            HandleNewIncomingConnection(*client);

            // Check if we need to notify deathmatch.so about the new player
            if (m_pendingPlayerJoin)
            {
                m_pendingPlayerJoin = false;

                // Copy data we need
                NetServerPlayerID joinPlayerID = m_pendingPlayerJoinPlayerID;
                uint16_t joinBitstreamVersion = m_pendingPlayerJoinBitstreamVersion;

                // Release lock
                lock.unlock();

                // QUEUE for main thread (instead of calling handler directly)
                NET_LOG("*** QUEUING PLAYER_JOIN for main thread ***");
                QueuePacketForMainThread(MTAPacketID::PLAYER_JOIN, joinPlayerID,
                                          nullptr, 0, joinBitstreamVersion);
                return;  // Exit early since lock is released
            }
            break;
        }

        case RakNetPacketID::DISCONNECTION_NOTIFICATION:
        case RakNetPacketID::CONNECTION_LOST:
        {
            NET_LOG("Client disconnected (packet 0x%02X): %s:%d",
                    packetID, inet_ntoa(fromAddr.sin_addr), ntohs(fromAddr.sin_port));

            // Copy data we need before releasing lock
            NetServerPlayerID copyPlayerID = client->playerID;
            uint16_t copyBitstreamVersion = client->bitstreamVersion;
            bool wasConnected = (client->state == ClientState::CONNECTED);

            // Remove client from map while holding lock
            m_clients.erase(key);
            NET_LOG("   Client removed from map");

            // Release lock
            lock.unlock();

            // QUEUE disconnect notification for main thread
            if (wasConnected)
            {
                NET_LOG("*** QUEUING DISCONNECTION for main thread ***");
                QueuePacketForMainThread(RakNetPacketID::DISCONNECTION_NOTIFICATION,
                                          copyPlayerID, nullptr, 0, copyBitstreamVersion);
            }

            NET_LOG("   Client disconnect handled successfully");
            return;  // Exit early since lock is released
        }

        // BYPASS: Sync packets - broadcast to other clients directly
        case SyncPacketID::PLAYER_PURESYNC:
        case SyncPacketID::PLAYER_VEHICLE_PURESYNC:
        case SyncPacketID::PLAYER_KEYSYNC:
        case SyncPacketID::LIGHTSYNC:
        {
            NET_LOG("   -> SYNC BYPASS: Broadcasting packet 0x%02X to other clients", packetID);
            NetServerPlayerID senderID = client->playerID;
            constexpr unsigned int kElementIdBits = 17;
            const uint32_t elementId = client->elementId;

            if (elementId == 0xFFFFFFFFu)
            {
                NET_LOG("   -> SYNC BYPASS: missing elementId for sender, queueing to main thread");
                NetServerPlayerID copyPlayerID = client->playerID;
                uint16_t copyBitstreamVersion = client->bitstreamVersion;
                lock.unlock();
                QueuePacketForMainThread(packetID, copyPlayerID,
                                          data + 1, length - 1, copyBitstreamVersion);
                return;
            }

            CNetBitStreamAndroid relayed;
            const uint32_t maxValue = (1u << kElementIdBits) - 1u;
            const uint32_t elementValue = (elementId == 0xFFFFFFFFu) ? maxValue : elementId;
            relayed.WriteBits(reinterpret_cast<const char*>(&elementValue), kElementIdBits);

            if (length > 1)
            {
                const unsigned int payloadBits = static_cast<unsigned int>((length - 1) * 8);
                relayed.WriteBits(reinterpret_cast<const char*>(data + 1), payloadBits);
            }

            const int relayedBytes = relayed.GetNumberOfBytesUsed();
            std::vector<uint8_t> packet(1 + relayedBytes);
            packet[0] = packetID;
            if (relayedBytes > 0)
            {
                memcpy(packet.data() + 1, relayed.GetData(), relayedBytes);
            }

            for (auto& pair : m_clients)
            {
                if (pair.second.state == ClientState::CONNECTED &&
                    !(pair.second.playerID == senderID))
                {
                    sockaddr_in destAddr;
                    destAddr.sin_family = AF_INET;
                    destAddr.sin_addr.s_addr = htonl(pair.second.playerID.GetBinaryAddress());
                    destAddr.sin_port = htons(pair.second.playerID.GetPort());

                    SendRawPacket(packet.data(), packet.size(), destAddr);
                    NET_LOG("      -> Sent to client %lu", pair.second.playerID.GetBinaryAddress());
                }
            }
            return;
        }

        default:
            // Pass to packet handler (game packets)
            NET_LOG("   -> Default handler for packet 0x%02X", packetID);
            if (client->state == ClientState::CONNECTED ||
                client->state == ClientState::AWAITING_JOINDATA)
            {
                // Copy data we need before releasing lock
                NetServerPlayerID copyPlayerID = client->playerID;
                uint16_t copyBitstreamVersion = client->bitstreamVersion;

                // Release lock
                lock.unlock();

                // QUEUE for main thread
                NET_LOG("*** QUEUING game packet 0x%02X for main thread ***", packetID);
                QueuePacketForMainThread(packetID, copyPlayerID,
                                          data + 1, length - 1, copyBitstreamVersion);
                return;  // Exit early since lock is released
            }
            break;
    }
    // Lock automatically released at end of function
}

//=============================================================================
// RakNet 3.x Handshake (Server Side)
//=============================================================================

void CNetServerAndroid::HandleOpenConnectionRequest(const uint8_t* data, int length,
                                                     const sockaddr_in& clientAddr,
                                                     ClientConnection& client)
{
    // NOTE: This function is called with the clients mutex held!

    // MTA RakNet 3.x OPEN_CONNECTION_REQUEST format:
    // 1 byte: packet ID (0x09)
    // 4 bytes: cookie (little-endian)

    if (length < 5)
    {
        NET_LOG("OPEN_CONNECTION_REQUEST too short: %d bytes", length);
        return;
    }

    // Read cookie (little-endian)
    uint32_t cookie = data[1] | (data[2] << 8) | (data[3] << 16) | (data[4] << 24);

    NET_LOG("OPEN_CONNECTION_REQUEST from %s:%d, cookie: 0x%08x",
            inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port), cookie);

    // Store cookie for this client
    client.cookie = cookie;
    client.state = ClientState::HANDSHAKE_OPEN_REQUEST;

    // Send OPEN_CONNECTION_REPLY
    uint8_t reply[5];
    reply[0] = RakNetPacketID::OPEN_CONNECTION_REPLY;
    // Echo cookie (little-endian)
    reply[1] = cookie & 0xFF;
    reply[2] = (cookie >> 8) & 0xFF;
    reply[3] = (cookie >> 16) & 0xFF;
    reply[4] = (cookie >> 24) & 0xFF;

    SendRawPacket(reply, sizeof(reply), clientAddr);
    NET_LOG("-> Sent OPEN_CONNECTION_REPLY");
}

void CNetServerAndroid::HandleConnectionRequest(const uint8_t* data, int length,
                                                 ClientConnection& client)
{
    // MTA RakNet 3.x CONNECTION_REQUEST format:
    // 1 byte: packet ID (0x04)
    // 8 bytes: client GUID (big-endian)
    // 8 bytes: timestamp (big-endian)
    // 1 byte: has security (0)

    if (length < 18)
    {
        NET_LOG("CONNECTION_REQUEST too short: %d bytes", length);
        return;
    }

    // Read client GUID (big-endian)
    uint64_t clientGUID = 0;
    for (int i = 0; i < 8; i++)
    {
        clientGUID = (clientGUID << 8) | data[1 + i];
    }

    // Read timestamp (big-endian)
    uint64_t timestamp = 0;
    for (int i = 0; i < 8; i++)
    {
        timestamp = (timestamp << 8) | data[9 + i];
    }

    NET_LOG("CONNECTION_REQUEST: client GUID: 0x%llx, timestamp: %llu",
            (unsigned long long)clientGUID, (unsigned long long)timestamp);

    client.guid = clientGUID;
    client.state = ClientState::HANDSHAKE_CONNECTION;
    client.connectTime = GetTimeMs();

    // Send CONNECTION_REQUEST_ACCEPTED
    uint8_t reply[96];
    memset(reply, 0, sizeof(reply));
    int offset = 0;

    // Packet ID
    reply[offset++] = RakNetPacketID::CONNECTION_REQUEST_ACCEPTED;

    // Client address (7 bytes: family + inverted IP + port)
    sockaddr_in clientAddr;
    clientAddr.sin_addr.s_addr = htonl(client.playerID.GetBinaryAddress());
    clientAddr.sin_port = htons(client.playerID.GetPort());

    reply[offset++] = 4;  // AF_INET
    // Inverted IP (RakNet quirk)
    uint32_t ip = ntohl(clientAddr.sin_addr.s_addr);
    reply[offset++] = ~((ip >> 24) & 0xFF);
    reply[offset++] = ~((ip >> 16) & 0xFF);
    reply[offset++] = ~((ip >> 8) & 0xFF);
    reply[offset++] = ~(ip & 0xFF);
    // Port (big-endian)
    reply[offset++] = (client.playerID.GetPort() >> 8) & 0xFF;
    reply[offset++] = client.playerID.GetPort() & 0xFF;

    // System index (2 bytes)
    reply[offset++] = 0;
    reply[offset++] = 0;

    // Internal addresses (10 addresses * 7 bytes = 70 bytes)
    for (int i = 0; i < 10; i++)
    {
        reply[offset++] = 4;  // AF_INET
        // 127.0.0.1 inverted
        reply[offset++] = 0x80;  // ~127
        reply[offset++] = 0xFF;  // ~0
        reply[offset++] = 0xFF;  // ~0
        reply[offset++] = 0xFE;  // ~1
        // Port 0
        reply[offset++] = 0;
        reply[offset++] = 0;
    }

    // Request time (8 bytes, big-endian) - echo client timestamp
    for (int i = 7; i >= 0; i--)
    {
        reply[offset++] = (timestamp >> (i * 8)) & 0xFF;
    }

    // Reply time (8 bytes, big-endian)
    uint64_t replyTime = GetTimeMs();
    for (int i = 7; i >= 0; i--)
    {
        reply[offset++] = (replyTime >> (i * 8)) & 0xFF;
    }

    // Send from player ID
    sockaddr_in destAddr;
    destAddr.sin_family = AF_INET;
    destAddr.sin_addr.s_addr = htonl(client.playerID.GetBinaryAddress());
    destAddr.sin_port = htons(client.playerID.GetPort());

    SendRawPacket(reply, offset, destAddr);
    NET_LOG("-> Sent CONNECTION_REQUEST_ACCEPTED (%d bytes)", offset);

    // Update state - waiting for joindata after MOD_NAME is sent
    client.state = ClientState::AWAITING_JOINDATA;

    // Send MOD_NAME directly - this is simpler and avoids ABI issues with calling deathmatch.so
    // deathmatch.so's Packet_PlayerJoin() also just sends MOD_NAME, so this is equivalent
    SendModName(client);
}

void CNetServerAndroid::HandleNewIncomingConnection(ClientConnection& client)
{
    NET_LOG("NEW_INCOMING_CONNECTION from client");

    // Client has confirmed connection - some clients send this, some don't
    // Note: We already set pending PLAYER_JOIN in HandleConnectionRequest
    // so if it hasn't been processed yet, it will be handled then.
    // If it was already processed, deathmatch.so already sent MOD_NAME.
    if (client.state != ClientState::AWAITING_JOINDATA)
    {
        client.state = ClientState::AWAITING_JOINDATA;

        // Set pending PLAYER_JOIN flag - will be processed after lock is released
        m_pendingPlayerJoin = true;
        m_pendingPlayerJoinPlayerID = client.playerID;
        m_pendingPlayerJoinBitstreamVersion = client.bitstreamVersion;
        NET_LOG("-> Marked PLAYER_JOIN pending (NEW_INCOMING_CONNECTION path)");
    }
}

//=============================================================================
// MTA Protocol
//=============================================================================

void CNetServerAndroid::SendModName(const ClientConnection& client)
{
    // MOD_NAME packet format:
    // 1 byte: packet ID (0x1C)
    // 2 bytes: bitstream version (little-endian)
    // 2 bytes: module name length (little-endian)
    // N bytes: module name ("deathmatch")

    const char* modName = "deathmatch";
    int modNameLen = strlen(modName);

    uint8_t packet[64];
    int offset = 0;

    packet[offset++] = WirePacketID::MOD_NAME;  // 0x1C on the wire

    // Bitstream version (little-endian)
    packet[offset++] = BITSTREAM_VERSION & 0xFF;
    packet[offset++] = (BITSTREAM_VERSION >> 8) & 0xFF;

    // Module name length (little-endian)
    packet[offset++] = modNameLen & 0xFF;
    packet[offset++] = (modNameLen >> 8) & 0xFF;

    // Module name
    memcpy(packet + offset, modName, modNameLen);
    offset += modNameLen;

    sockaddr_in destAddr;
    destAddr.sin_family = AF_INET;
    destAddr.sin_addr.s_addr = htonl(client.playerID.GetBinaryAddress());
    destAddr.sin_port = htons(client.playerID.GetPort());

    SendRawPacket(packet, offset, destAddr);
    NET_LOG("-> Sent MOD_NAME (deathmatch, version 0x%03x)", BITSTREAM_VERSION);
}

void CNetServerAndroid::HandlePlayerJoinData(const uint8_t* data, int length,
                                              ClientConnection& client)
{
    // NOTE: This function is called with the clients mutex held!
    // DO NOT call m_pfnPacketHandler here - the caller handles that after releasing the lock.

    NET_LOG("Received PLAYER_JOINDATA (%d bytes)", length);

    // Parse join data to extract bitstream version and player name
    if (length >= 7)
    {
        int offset = 1;  // Skip packet ID

        // Netcode version
        uint16_t netcodeVersion = data[offset] | (data[offset + 1] << 8);
        offset += 2;

        // MTA version
        uint16_t mtaVersion = data[offset] | (data[offset + 1] << 8);
        offset += 2;

        // Bitstream version
        uint16_t bitstreamVersion = data[offset] | (data[offset + 1] << 8);
        offset += 2;

        NET_LOG("   Netcode: 0x%04x, MTA: 0x%04x, Bitstream: 0x%04x",
                netcodeVersion, mtaVersion, bitstreamVersion);

        client.bitstreamVersion = bitstreamVersion;

        // Skip version string (uint16_t length + string data)
        if (offset + 2 <= length)
        {
            uint16_t versionStringLen = data[offset] | (data[offset + 1] << 8);
            offset += 2;
            offset += versionStringLen;  // Skip string data

            // Skip game version (1 byte)
            if (offset + 1 <= length)
            {
                offset += 1;

                // Read nickname (22 bytes fixed)
                if (offset + 22 <= length)
                {
                    char nickname[23] = {0};
                    memcpy(nickname, data + offset, 22);
                    // Find actual string length (null-terminated or full 22 chars)
                    size_t nickLen = strnlen(nickname, 22);
                    client.playerName = std::string(nickname, nickLen);
                    NET_LOG("   Player name: '%s'", client.playerName.c_str());
                }
            }
        }
    }

    // Mark as connected and assign unique player ID
    client.state = ClientState::CONNECTED;
    client.gamePlayerId = m_nextGamePlayerId++;
    NET_LOG("   Assigned gamePlayerId: %d", client.gamePlayerId);

    // OPTION D: Send JOIN_COMPLETE directly instead of waiting for deathmatch.so
    // This bypasses the unreliable path through deathmatch.so -> SendPacket
    NET_LOG("*** CLIENT JOINDATA RECEIVED - sending JOIN_COMPLETE directly ***");
    SendJoinComplete(client);

    // Send existing players to the new client
    SendExistingPlayersTo(client);

    // Notify existing clients about the new player (PLAYER_JOIN)
    NotifyPlayerJoined(client);

    // Send spawn info for existing players to the new client
    SendPlayerSpawn(client);

    // Send spawn info for the new player to existing clients
    SendNewPlayerSpawnToExisting(client);
}

void CNetServerAndroid::SendJoinComplete(ClientConnection& client)
{
    // SERVER_JOIN_COMPLETE packet format:
    // 1 byte: packet ID (0x02)
    // 2 bytes: version string length (little-endian)
    // N bytes: version string

    const char* version = "1.6.0";
    int versionLen = strlen(version);

    uint8_t packet[64];
    int offset = 0;

    packet[offset++] = WirePacketID::SERVER_JOIN_COMPLETE;  // 0x02 on the wire

    // Version string length (little-endian)
    packet[offset++] = versionLen & 0xFF;
    packet[offset++] = (versionLen >> 8) & 0xFF;

    // Version string
    memcpy(packet + offset, version, versionLen);
    offset += versionLen;

    // Full version info
    const char* fullVersion = "MTA:SA Server v1.6.0 (Android)";
    int fullVersionLen = strlen(fullVersion);

    packet[offset++] = fullVersionLen & 0xFF;
    packet[offset++] = (fullVersionLen >> 8) & 0xFF;
    memcpy(packet + offset, fullVersion, fullVersionLen);
    offset += fullVersionLen;

    sockaddr_in destAddr;
    destAddr.sin_family = AF_INET;
    destAddr.sin_addr.s_addr = htonl(client.playerID.GetBinaryAddress());
    destAddr.sin_port = htons(client.playerID.GetPort());

    SendRawPacket(packet, offset, destAddr);
    NET_LOG("-> Sent JOIN_COMPLETE");

    // Also send JOINED_GAME
    uint8_t joinedPacket[10];
    offset = 0;

    joinedPacket[offset++] = WirePacketID::SERVER_JOINEDGAME;  // 0x16 on the wire

    // Player ID (little-endian uint16) - use actual assigned ID
    uint16_t playerIndex = client.gamePlayerId;
    joinedPacket[offset++] = playerIndex & 0xFF;
    joinedPacket[offset++] = (playerIndex >> 8) & 0xFF;

    // Player count
    joinedPacket[offset++] = 1;

    // Root element ID (little-endian uint16)
    joinedPacket[offset++] = 1;
    joinedPacket[offset++] = 0;

    SendRawPacket(joinedPacket, offset, destAddr);
    NET_LOG("-> Sent JOINED_GAME (player ID: %d)", playerIndex);
}

void CNetServerAndroid::SendPlayerSpawn(ClientConnection& newClient)
{
    // Send PLAYER_SPAWN for each existing player to the new client
    // NOTE: Called while m_clientsMutex is already held by caller

    sockaddr_in destAddr;
    destAddr.sin_family = AF_INET;
    destAddr.sin_addr.s_addr = htonl(newClient.playerID.GetBinaryAddress());
    destAddr.sin_port = htons(newClient.playerID.GetPort());

    for (auto& pair : m_clients)
    {
        ClientConnection& existingClient = pair.second;

        // Skip if not connected or if it's the new client itself
        if (existingClient.state != ClientState::CONNECTED ||
            existingClient.playerID == newClient.playerID)
        {
            continue;
        }

        uint8_t packet[128];
        int offset = 0;

        packet[offset++] = 0x18;  // PLAYER_SPAWN

        // Player ID (2 bytes, little-endian) - use actual player ID
        uint16_t playerId = existingClient.gamePlayerId;
        packet[offset++] = playerId & 0xFF;
        packet[offset++] = (playerId >> 8) & 0xFF;

        // Model ID (2 bytes, little-endian) - 0 = CJ
        packet[offset++] = 0;
        packet[offset++] = 0;

        // Team ID (2 bytes) - 0xFFFF = no team
        packet[offset++] = 0xFF;
        packet[offset++] = 0xFF;

        // Position (3 floats) - spawn position
        float posX = 2245.0f, posY = -1261.0f, posZ = 24.0f;
        memcpy(packet + offset, &posX, 4); offset += 4;
        memcpy(packet + offset, &posY, 4); offset += 4;
        memcpy(packet + offset, &posZ, 4); offset += 4;

        // Rotation (float)
        float rot = 0.0f;
        memcpy(packet + offset, &rot, 4); offset += 4;

        // Health (float)
        float health = 100.0f;
        memcpy(packet + offset, &health, 4); offset += 4;

        // Armor (float)
        float armor = 0.0f;
        memcpy(packet + offset, &armor, 4); offset += 4;

        // Nickname - use actual player name
        const char* nick = existingClient.playerName.empty() ? "Player" : existingClient.playerName.c_str();
        uint8_t nickLen = strlen(nick);
        if (nickLen > 64) nickLen = 64;  // Safety limit
        packet[offset++] = nickLen;
        memcpy(packet + offset, nick, nickLen);
        offset += nickLen;

        SendRawPacket(packet, offset, destAddr);
        NET_LOG("-> Sent PLAYER_SPAWN for player %d (%s) to new client", playerId, nick);
    }
}

void CNetServerAndroid::SendNewPlayerSpawnToExisting(ClientConnection& newPlayer)
{
    // Send PLAYER_SPAWN for the new player to all existing clients
    // NOTE: Called while m_clientsMutex is already held by caller

    // Build the spawn packet for the new player
    uint8_t packet[128];
    int offset = 0;

    packet[offset++] = 0x18;  // PLAYER_SPAWN

    // Player ID (2 bytes, little-endian) - use new player's actual ID
    uint16_t playerId = newPlayer.gamePlayerId;
    packet[offset++] = playerId & 0xFF;
    packet[offset++] = (playerId >> 8) & 0xFF;

    // Model ID (2 bytes, little-endian) - 0 = CJ
    packet[offset++] = 0;
    packet[offset++] = 0;

    // Team ID (2 bytes) - 0xFFFF = no team
    packet[offset++] = 0xFF;
    packet[offset++] = 0xFF;

    // Position (3 floats) - spawn position
    float posX = 2245.0f, posY = -1261.0f, posZ = 24.0f;
    memcpy(packet + offset, &posX, 4); offset += 4;
    memcpy(packet + offset, &posY, 4); offset += 4;
    memcpy(packet + offset, &posZ, 4); offset += 4;

    // Rotation (float)
    float rot = 0.0f;
    memcpy(packet + offset, &rot, 4); offset += 4;

    // Health (float)
    float health = 100.0f;
    memcpy(packet + offset, &health, 4); offset += 4;

    // Armor (float)
    float armor = 0.0f;
    memcpy(packet + offset, &armor, 4); offset += 4;

    // Nickname - use actual player name
    const char* nick = newPlayer.playerName.empty() ? "Player" : newPlayer.playerName.c_str();
    uint8_t nickLen = strlen(nick);
    if (nickLen > 64) nickLen = 64;  // Safety limit
    packet[offset++] = nickLen;
    memcpy(packet + offset, nick, nickLen);
    offset += nickLen;

    // Send to all existing connected clients
    for (auto& pair : m_clients)
    {
        ClientConnection& existingClient = pair.second;

        // Skip if not connected or if it's the new player itself
        if (existingClient.state != ClientState::CONNECTED ||
            existingClient.playerID == newPlayer.playerID)
        {
            continue;
        }

        sockaddr_in destAddr;
        destAddr.sin_family = AF_INET;
        destAddr.sin_addr.s_addr = htonl(existingClient.playerID.GetBinaryAddress());
        destAddr.sin_port = htons(existingClient.playerID.GetPort());

        SendRawPacket(packet, offset, destAddr);
        NET_LOG("-> Sent PLAYER_SPAWN for new player %d (%s) to existing client %d",
                playerId, nick, existingClient.gamePlayerId);
    }
}

void CNetServerAndroid::NotifyPlayerJoined(ClientConnection& newPlayer)
{
    // Send PLAYER_JOIN (0x03 = 3) to all existing clients
    // Format: playerId (16 bits), nickLen (8 bits), nick (N bytes)
    // NOTE: Called while m_clientsMutex is already held by caller

    uint8_t packet[64];
    int offset = 0;

    packet[offset++] = 0x03;  // PLAYER_JOIN (3 in client's PacketID enum)

    // Player ID (2 bytes) - use actual player ID
    uint16_t playerId = newPlayer.gamePlayerId;
    packet[offset++] = playerId & 0xFF;
    packet[offset++] = (playerId >> 8) & 0xFF;

    // Nickname - use actual player name
    const char* nick = newPlayer.playerName.empty() ? "Player" : newPlayer.playerName.c_str();
    uint8_t nickLen = strlen(nick);
    if (nickLen > 64) nickLen = 64;  // Safety limit
    packet[offset++] = nickLen;
    memcpy(packet + offset, nick, nickLen);
    offset += nickLen;

    // Send to all connected clients except the new one
    for (auto& pair : m_clients)
    {
        if (pair.second.state == ClientState::CONNECTED &&
            !(pair.second.playerID == newPlayer.playerID))
        {
            sockaddr_in destAddr;
            destAddr.sin_family = AF_INET;
            destAddr.sin_addr.s_addr = htonl(pair.second.playerID.GetBinaryAddress());
            destAddr.sin_port = htons(pair.second.playerID.GetPort());

            SendRawPacket(packet, offset, destAddr);
            NET_LOG("-> Sent PLAYER_JOIN for player %d (%s) to existing client", playerId, nick);
        }
    }
}

void CNetServerAndroid::SendExistingPlayersTo(ClientConnection& newPlayer)
{
    // Send PLAYER_JOIN for each existing player to the new client
    // NOTE: Called while m_clientsMutex is already held by caller

    sockaddr_in destAddr;
    destAddr.sin_family = AF_INET;
    destAddr.sin_addr.s_addr = htonl(newPlayer.playerID.GetBinaryAddress());
    destAddr.sin_port = htons(newPlayer.playerID.GetPort());

    for (auto& pair : m_clients)
    {
        ClientConnection& existingClient = pair.second;

        // Skip if not connected or if it's the new client itself
        if (existingClient.state != ClientState::CONNECTED ||
            existingClient.playerID == newPlayer.playerID)
        {
            continue;
        }

        // Send PLAYER_JOIN for this existing player
        uint8_t packet[64];
        int offset = 0;

        packet[offset++] = 0x03;  // PLAYER_JOIN (3 in client's PacketID enum)

        // Player ID - use actual player ID
        uint16_t playerId = existingClient.gamePlayerId;
        packet[offset++] = playerId & 0xFF;
        packet[offset++] = (playerId >> 8) & 0xFF;

        // Nickname - use actual player name
        const char* nick = existingClient.playerName.empty() ? "Player" : existingClient.playerName.c_str();
        uint8_t nickLen = strlen(nick);
        if (nickLen > 64) nickLen = 64;  // Safety limit
        packet[offset++] = nickLen;
        memcpy(packet + offset, nick, nickLen);
        offset += nickLen;

        SendRawPacket(packet, offset, destAddr);
        NET_LOG("-> Sent existing player %d (%s) to new client", playerId, nick);
    }
}

//=============================================================================
// Client Management
//=============================================================================

ClientConnection* CNetServerAndroid::GetClient(const NetServerPlayerID& playerID)
{
    uint64_t key = ((uint64_t)playerID.GetBinaryAddress() << 16) | playerID.GetPort();

    std::lock_guard<std::mutex> lock(m_clientsMutex);
    auto it = m_clients.find(key);
    if (it != m_clients.end())
    {
        return &it->second;
    }
    return nullptr;
}

ClientConnection* CNetServerAndroid::GetOrCreateClient(const sockaddr_in& addr)
{
    NetServerPlayerID playerID = MakePlayerID(addr);
    uint64_t key = ((uint64_t)playerID.GetBinaryAddress() << 16) | playerID.GetPort();

    std::lock_guard<std::mutex> lock(m_clientsMutex);
    auto it = m_clients.find(key);
    if (it != m_clients.end())
    {
        return &it->second;
    }

    // Create new client
    ClientConnection client;
    client.playerID = playerID;
    client.state = ClientState::DISCONNECTED;
    client.connectTime = GetTimeMs();
    client.lastPacketTime = GetTimeMs();

    m_clients[key] = client;
    return &m_clients[key];
}

void CNetServerAndroid::RemoveClient(const NetServerPlayerID& playerID)
{
    uint64_t key = ((uint64_t)playerID.GetBinaryAddress() << 16) | playerID.GetPort();

    std::lock_guard<std::mutex> lock(m_clientsMutex);
    m_clients.erase(key);
}

//=============================================================================
// Packet Handler
//=============================================================================

void CNetServerAndroid::RegisterPacketHandler(PPACKETHANDLER pfnPacketHandler)
{
    m_pfnPacketHandler = pfnPacketHandler;
    NET_LOG("Packet handler registered");
}

//=============================================================================
// Packet Queue - Thread-Safe Processing
// Network thread queues packets, main thread (DoPulse) processes them
//=============================================================================

void CNetServerAndroid::QueuePacketForMainThread(uint8_t packetID,
                                                   const NetServerPlayerID& playerID,
                                                   const uint8_t* data, int length,
                                                   uint16_t bitstreamVersion)
{
    QueuedPacket packet;
    packet.packetID = packetID;
    packet.playerID = playerID;
    packet.bitstreamVersion = bitstreamVersion;
    packet.hasPing = true;
    packet.ping = 50;  // Placeholder

    // Copy packet data
    if (data && length > 0)
    {
        packet.data.resize(length);
        memcpy(packet.data.data(), data, length);
    }

    // Add to queue (thread-safe)
    size_t queueSize;
    {
        std::lock_guard<std::mutex> lock(m_packetQueueMutex);
        m_packetQueue.push_back(std::move(packet));
        queueSize = m_packetQueue.size();
    }

    NET_LOG("*** QUEUED packet ID=%d for main thread (queue size now: %zu) ***",
            packetID, queueSize);
}

void CNetServerAndroid::ProcessQueuedPackets()
{
    // Swap the queue to minimize lock time
    std::vector<QueuedPacket> packetsToProcess;
    {
        std::lock_guard<std::mutex> lock(m_packetQueueMutex);
        if (m_packetQueue.empty())
            return;
        packetsToProcess.swap(m_packetQueue);
    }

    NET_LOG("*** PROCESSING %zu queued packets in MAIN THREAD ***", packetsToProcess.size());

    // Process each packet (now in main thread - safe for deathmatch.so)
    for (const auto& packet : packetsToProcess)
    {
        if (!m_pfnPacketHandler)
        {
            NET_LOG("*** ERROR: No packet handler registered! ***");
            continue;
        }

        NET_LOG("*** MAIN THREAD: Processing packet ID=%d ***", packet.packetID);

        // Create bitstream from queued data
        auto* bitStream = new CNetBitStreamAndroid(
            packet.data.empty() ? nullptr : packet.data.data(),
            packet.data.size(),
            packet.bitstreamVersion);

        auto* extraInfo = new SNetExtraInfo();
        extraInfo->m_bHasPing = packet.hasPing;
        extraInfo->m_uiPing = packet.ping;

        NET_LOG("*** MAIN THREAD: Calling deathmatch.so handler (packetID=%d, version=0x%04X) ***",
                packet.packetID, packet.bitstreamVersion);

        // Call the packet handler - THIS IS NOW IN THE MAIN THREAD
        m_pfnPacketHandler(packet.packetID, packet.playerID, bitStream, extraInfo);

        NET_LOG("*** MAIN THREAD: Handler returned successfully ***");

        // TEMPORARY: Don't release bitstream immediately - deathmatch.so may still be using it
        // on a worker thread. This leaks memory but will confirm use-after-free theory.
        // TODO: Implement proper reference counting coordination with deathmatch.so
        // bitStream->Release();
        // extraInfo->Release();
    }
}

//=============================================================================
// Sending Packets
//=============================================================================

bool CNetServerAndroid::SendPacket(unsigned char ucPacketID,
                                    const NetServerPlayerID& playerID,
                                    NetBitStreamInterface* bitStream,
                                    bool bBroadcast,
                                    NetServerPacketPriority packetPriority,
                                    NetServerPacketReliability packetReliability,
                                    ePacketOrdering packetOrdering)
{
    // Check if network is started
    if (!m_bRunning || m_socket < 0)
    {
        NET_LOG("SendPacket called before network started, ignoring");
        return true;  // Return true to avoid error handling in caller
    }

    if (!bitStream)
        return false;

    // Convert internal packet IDs to wire format
    // deathmatch.so uses internal IDs, but the Android client expects wire format
    unsigned char wirePacketID = ucPacketID;
    switch (ucPacketID)
    {
        case MTAPacketID::MOD_NAME:  // 7 -> 0x1C
            wirePacketID = WirePacketID::MOD_NAME;
            NET_LOG("SendPacket: Converting MOD_NAME (7 -> 0x1C)");
            break;
        case MTAPacketID::SERVER_JOIN_COMPLETE:  // 2 -> 0x02 (same, but log it)
            wirePacketID = WirePacketID::SERVER_JOIN_COMPLETE;
            NET_LOG("SendPacket: SERVER_JOIN_COMPLETE (2 -> 0x02) to %lu:%u",
                    playerID.GetBinaryAddress(), playerID.GetPort());
            break;
        case MTAPacketID::SERVER_JOINEDGAME:  // 22 -> 0x16
            wirePacketID = WirePacketID::SERVER_JOINEDGAME;
            NET_LOG("SendPacket: Converting SERVER_JOINEDGAME (22 -> 0x16)");
            break;
        default:
            NET_LOG("SendPacket: Packet ID 0x%02X (no conversion)", ucPacketID);
            break;
    }

    if (ucPacketID == MTAPacketID::SERVER_JOINEDGAME)
    {
        constexpr unsigned int kElementIdBits = 17;
        const int dataSize = bitStream->GetNumberOfBytesUsed();
        if (dataSize > 0 && (dataSize * 8) >= static_cast<int>(kElementIdBits))
        {
            CNetBitStreamAndroid probe(bitStream->GetData(), static_cast<unsigned int>(dataSize));
            uint32_t rawId = 0;
            if (probe.ReadBits(reinterpret_cast<char*>(&rawId), kElementIdBits))
            {
                const uint32_t maxValue = (1u << kElementIdBits) - 1u;
                uint32_t elementId = (rawId == maxValue) ? 0xFFFFFFFFu : rawId;
                if (auto* client = GetClient(playerID))
                {
                    client->elementId = elementId;
                    NET_LOG("SendPacket: cached elementId %u for %lu:%u",
                            elementId,
                            playerID.GetBinaryAddress(),
                            playerID.GetPort());
                }
            }
        }
    }

    // Build packet: ID + data
    int dataSize = bitStream->GetNumberOfBytesUsed();
    std::vector<uint8_t> packet(1 + dataSize);
    packet[0] = wirePacketID;

    if (dataSize > 0)
    {
        memcpy(packet.data() + 1, bitStream->GetData(), dataSize);
    }

    if (bBroadcast)
    {
        // Send to all connected clients
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        for (auto& pair : m_clients)
        {
            if (pair.second.state == ClientState::CONNECTED)
            {
                sockaddr_in destAddr;
                destAddr.sin_family = AF_INET;
                destAddr.sin_addr.s_addr = htonl(pair.second.playerID.GetBinaryAddress());
                destAddr.sin_port = htons(pair.second.playerID.GetPort());

                SendRawPacket(packet.data(), packet.size(), destAddr);
                pair.second.packetsSent++;
                pair.second.bytesSent += packet.size();
            }
        }
    }
    else
    {
        // Send to specific client
        // Use latest known port for this IP (handles NAT port changes)
        uint32_t ip = playerID.GetBinaryAddress();
        uint16_t port = playerID.GetPort();

        {
            std::lock_guard<std::mutex> portLock(m_portMapMutex);
            auto it = m_ipToLatestPort.find(ip);
            if (it != m_ipToLatestPort.end() && it->second != port)
            {
                NET_LOG("SendPacket: Correcting port %u -> %u for IP %08X",
                        port, it->second, ip);
                port = it->second;
            }
        }

        sockaddr_in destAddr;
        destAddr.sin_family = AF_INET;
        destAddr.sin_addr.s_addr = htonl(ip);
        destAddr.sin_port = htons(port);

        NET_LOG("SendPacket: Sending %zu bytes to %s:%d (packet 0x%02X)",
                packet.size(), inet_ntoa(destAddr.sin_addr), ntohs(destAddr.sin_port), wirePacketID);

        SendRawPacket(packet.data(), packet.size(), destAddr);

        auto* client = GetClient(playerID);
        if (client)
        {
            client->packetsSent++;
            client->bytesSent += packet.size();
        }
    }

    // Update stats (outgoing traffic)
    m_packetStats[STATS_OUTGOING_TRAFFIC][ucPacketID].iCount++;
    m_packetStats[STATS_OUTGOING_TRAFFIC][ucPacketID].iTotalBytes += packet.size();

    return true;
}

//=============================================================================
// BitStream Management
//=============================================================================

NetBitStreamInterface* CNetServerAndroid::AllocateNetServerBitStream(
    unsigned short usBitStreamVersion,
    const void* pData,
    uint uiDataSize,
    bool bCopyData)
{
    return new CNetBitStreamAndroid(pData, uiDataSize, usBitStreamVersion);
}

void CNetServerAndroid::DeallocateNetServerBitStream(NetBitStreamInterface* bitStream)
{
    if (bitStream)
    {
        bitStream->Release();
    }
}

//=============================================================================
// Player Information
//=============================================================================

void CNetServerAndroid::GetPlayerIP(const NetServerPlayerID& playerID,
                                     char strIP[22], unsigned short* usPort)
{
    in_addr addr;
    addr.s_addr = htonl(playerID.GetBinaryAddress());
    const char* ip = inet_ntoa(addr);

    if (ip)
    {
        strncpy(strIP, ip, 21);
        strIP[21] = '\0';
    }
    else
    {
        strcpy(strIP, "0.0.0.0");
    }

    if (usPort)
    {
        *usPort = playerID.GetPort();
    }
}

void CNetServerAndroid::Kick(const NetServerPlayerID& PlayerID)
{
    auto* client = GetClient(PlayerID);
    if (!client)
        return;

    // Send disconnect notification
    uint8_t packet[1] = { RakNetPacketID::DISCONNECTION_NOTIFICATION };

    sockaddr_in destAddr;
    destAddr.sin_family = AF_INET;
    destAddr.sin_addr.s_addr = htonl(PlayerID.GetBinaryAddress());
    destAddr.sin_port = htons(PlayerID.GetPort());

    SendRawPacket(packet, sizeof(packet), destAddr);

    NET_LOG("Kicked client %s:%d",
            inet_ntoa(destAddr.sin_addr), ntohs(destAddr.sin_port));

    RemoveClient(PlayerID);
}

//=============================================================================
// Statistics
//=============================================================================

bool CNetServerAndroid::GetNetworkStatistics(NetStatistics* pDest,
                                              const NetServerPlayerID& PlayerID)
{
    if (!pDest)
        return false;

    memset(pDest, 0, sizeof(NetStatistics));

    auto* client = GetClient(PlayerID);
    if (client)
    {
        pDest->bytesReceived = client->bytesReceived;
        pDest->bytesSent = client->bytesSent;
        pDest->packetsReceived = client->packetsReceived;
        pDest->packetsSent = client->packetsSent;
    }

    return true;
}

const SPacketStat* CNetServerAndroid::GetPacketStats()
{
    // Return pointer to start of 2D array (treated as contiguous SPacketStat[512])
    return &m_packetStats[0][0];
}

bool CNetServerAndroid::GetBandwidthStatistics(SBandwidthStatistics* pDest)
{
    if (!pDest)
        return false;

    memset(pDest, 0, sizeof(SBandwidthStatistics));

    // Aggregate stats from all clients
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    for (const auto& pair : m_clients)
    {
        pDest->llIncomingUDPByteCount += pair.second.bytesReceived;
        pDest->llOutgoingUDPByteCount += pair.second.bytesSent;
        pDest->llIncomingUDPPacketCount += pair.second.packetsReceived;
        pDest->llOutgoingUDPPacketCount += pair.second.packetsSent;
    }

    return true;
}

bool CNetServerAndroid::GetNetPerformanceStatistics(SNetPerformanceStatistics* pDest,
                                                     bool bResetCounters)
{
    if (!pDest)
        return false;

    memset(pDest, 0, sizeof(SNetPerformanceStatistics));
    return true;
}

void CNetServerAndroid::GetPingStatus(SFixedString<32>* pstrStatus)
{
    if (pstrStatus)
    {
        // Use raw pointer to avoid ABI issues
        char* ptr = reinterpret_cast<char*>(pstrStatus);
        strncpy(ptr, "OK", 31);
        ptr[31] = '\0';
    }
}

bool CNetServerAndroid::GetSyncThreadStatistics(SSyncThreadStatistics* pDest,
                                                 bool bResetCounters)
{
    if (!pDest)
        return false;

    memset(pDest, 0, sizeof(SSyncThreadStatistics));
    return true;
}

//=============================================================================
// Configuration
//=============================================================================

void CNetServerAndroid::SetPassword(const char* szPassword)
{
    m_strPassword = szPassword ? szPassword : "";
}

void CNetServerAndroid::SetMaximumIncomingConnections(unsigned short numberAllowed)
{
    m_uiMaxPlayers = numberAllowed;
}

CNetHTTPDownloadManagerInterface* CNetServerAndroid::GetHTTPDownloadManager(
    EDownloadModeType iMode)
{
    // Return a stub download manager to avoid null pointer crashes
    // Android doesn't actually use HTTP downloads for resources
    static CNetHTTPDownloadManagerStub stubManager;
    return &stubManager;
}

void CNetServerAndroid::SetClientBitStreamVersion(const NetServerPlayerID& PlayerID,
                                                   unsigned short usBitStreamVersion)
{
    auto* client = GetClient(PlayerID);
    if (client)
    {
        client->bitstreamVersion = usBitStreamVersion;
    }
}

void CNetServerAndroid::ClearClientBitStreamVersion(const NetServerPlayerID& PlayerID)
{
    // Nothing to do
}

void CNetServerAndroid::SetChecks(const char* szDisableComboACMap,
                                   const char* szDisableACMap,
                                   const char* szEnableSDMap,
                                   int iEnableClientChecks,
                                   bool bHideAC,
                                   const char* szImgMods)
{
    // Anti-cheat is disabled for Android clients
    NET_LOG("SetChecks called (AC disabled for Android)");
}

unsigned int CNetServerAndroid::GetPendingPacketCount()
{
    return 0;
}

void CNetServerAndroid::GetNetRoute(SFixedString<32>* pstrRoute)
{
    if (pstrRoute)
    {
        // Use raw pointer to avoid ABI issues
        char* ptr = reinterpret_cast<char*>(pstrRoute);
        strncpy(ptr, "direct", 31);
        ptr[31] = '\0';
    }
}

bool CNetServerAndroid::InitServerId(const char* szPath)
{
    // Generate server ID if needed
    return true;
}

void CNetServerAndroid::ResendModPackets(const NetServerPlayerID& playerID)
{
    auto* client = GetClient(playerID);
    if (client)
    {
        SendModName(*client);
    }
}

void CNetServerAndroid::ResendACPackets(const NetServerPlayerID& playerID)
{
    // No AC packets for Android
}

void CNetServerAndroid::GetClientSerialAndVersion(const NetServerPlayerID& playerID,
                                                   SFixedString<32>& strSerial,
                                                   SFixedString<64>& strExtra,
                                                   SFixedString<32>& strVersion)
{
    // Default serial - MUST be exactly 32 hex characters (A-F, 0-9 only!)
    // "ANDROID" is INVALID because N,D,R,I are not hex
    // Use A1D01D prefix (looks like ANDROID in hex-speak)
    strSerial = "A1D01D00000000000000000000000000";  // 32 hex chars
    strExtra = "";
    strVersion = "1.6.0";

    auto* client = GetClient(playerID);
    if (client)
    {
        // Create serial from GUID - pure hex format!
        // Use A1D01D prefix (8 chars) + 24 hex chars from GUID = 32 hex chars
        char serial[33];
        snprintf(serial, sizeof(serial), "A1D01D00%024llX",
                 (unsigned long long)client->guid);
        strSerial = serial;

        NET_LOG("Generated serial for client: %s", static_cast<const char*>(strSerial));
    }
}

void CNetServerAndroid::SetNetOptions(const SNetOptions& options)
{
    // Store options if needed
}

void CNetServerAndroid::GenerateRandomData(void* pOutData, uint uiLength)
{
    if (!pOutData || uiLength == 0)
        return;

    uint8_t* data = static_cast<uint8_t*>(pOutData);
    for (uint i = 0; i < uiLength; i++)
    {
        data[i] = rand() & 0xFF;
    }
}

bool CNetServerAndroid::IsValidSocket(const NetServerPlayerID& playerID)
{
    return GetClient(playerID) != nullptr;
}

//=============================================================================
// Utility
//=============================================================================

NetServerPlayerID CNetServerAndroid::MakePlayerID(const sockaddr_in& addr)
{
    return NetServerPlayerID(ntohl(addr.sin_addr.s_addr), ntohs(addr.sin_port));
}

bool CNetServerAndroid::SendRawPacket(const uint8_t* data, int length,
                                       const sockaddr_in& addr)
{
    ssize_t sent = sendto(m_socket, (const char*)data, length, 0,
                          (const sockaddr*)&addr, sizeof(addr));

    if (sent != length)
    {
        NET_LOG("sendto failed: %s", strerror(errno));
        return false;
    }

    LogPacket("->", data, length, addr);
    return true;
}

uint64_t CNetServerAndroid::GetTimeMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(
        steady_clock::now().time_since_epoch()
    ).count();
}

void CNetServerAndroid::LogPacket(const char* direction, const uint8_t* data,
                                   int length, const sockaddr_in& addr)
{
    auto shouldDumpFull = [](const uint8_t* payload, int payloadLength) {
        static int s_dump = -1;
        if (s_dump == -1)
        {
            const char* env = getenv("MTA_ANDROID_DUMP_PACKETS");
            s_dump = (env && *env && *env != '0') ? 1 : 0;
        }

        if (!s_dump || !payload || payloadLength <= 0)
            return false;

        const uint8_t id = payload[0];
        switch (id)
        {
            case SyncPacketID::PLAYER_LIST:
            case 26:  // PLAYER_SPAWN
            case SyncPacketID::PLAYER_KEYSYNC:
            case SyncPacketID::PLAYER_PURESYNC:
            case SyncPacketID::PLAYER_VEHICLE_PURESYNC:
            case SyncPacketID::LIGHTSYNC:
                return true;
            default:
                return false;
        }
    };

    if (shouldDumpFull(data, length))
    {
        NET_LOG("%s [%s:%d] %d bytes:",
                direction,
                inet_ntoa(addr.sin_addr),
                ntohs(addr.sin_port),
                length);

        for (int offset = 0; offset < length; offset += 16)
        {
            char hexLine[64];
            int hexLen = 0;
            int maxBytes = std::min(16, length - offset);
            for (int i = 0; i < maxBytes && hexLen < (int)sizeof(hexLine) - 3; i++)
            {
                hexLen += snprintf(hexLine + hexLen, sizeof(hexLine) - hexLen,
                                   "%02x ", data[offset + i]);
            }
            NET_LOG("  %04x: %s", offset, hexLine);
        }
        return;
    }

    // Default: print first 32 bytes only
    char hexStr[128];
    int hexLen = 0;
    int maxBytes = std::min(length, 32);

    for (int i = 0; i < maxBytes && hexLen < 120; i++)
    {
        hexLen += snprintf(hexStr + hexLen, sizeof(hexStr) - hexLen,
                           "%02x ", data[i]);
    }

    if (length > maxBytes)
    {
        snprintf(hexStr + hexLen, sizeof(hexStr) - hexLen, "...");
    }

    NET_LOG("%s [%s:%d] %d bytes: %s",
            direction,
            inet_ntoa(addr.sin_addr),
            ntohs(addr.sin_port),
            length,
            hexStr);
}

//=============================================================================
// Module Instance
//=============================================================================

static CNetServerAndroid* g_pNetServer = nullptr;

#ifdef _WIN32
    #define MTAEXPORT extern "C" __declspec(dllexport)
#else
    #define MTAEXPORT extern "C" __attribute__((visibility("default")))
#endif

// Module version - must match the server's expected version
// VPS server (1.6.0 build 23183) expects 0x0AB
#define NET_ANDROID_MODULE_VERSION 0x0AB

// MTA version string - must match the server's expected version
// Format: MAJOR.MINOR.MAINTENANCE-TYPE.BUILD.0
// The VPS server expects "1.6.0-9.23183.0" (release build 23183)
#define MTA_VERSION_STRING "1.6.0-9.23183.0"

// GetLibMtaVersion - called by MTA server to verify library version string
MTAEXPORT void GetLibMtaVersion(char* pBuffer, unsigned int uiMaxSize)
{
    const char* version = MTA_VERSION_STRING;
    if (pBuffer && uiMaxSize > 0)
    {
        strncpy(pBuffer, version, uiMaxSize - 1);
        pBuffer[uiMaxSize - 1] = '\0';
    }
}

// CheckCompatibility - called by MTA server to verify module version
// Note: Server calls with (MTA_DM_SERVER_NET_MODULE_VERSION, (unsigned long*)MTASA_VERSION_TYPE)
// where MTASA_VERSION_TYPE is a small integer (9), not a valid pointer!
// We must NOT dereference pulOutVersion unless called with version=1 for error info.
MTAEXPORT unsigned long CheckCompatibility(unsigned long ulExpectedVersion, unsigned long* pulOutVersion)
{
    // If called with version 1, the server wants our version for error message
    // In this case, pulOutVersion is a real pointer
    if (ulExpectedVersion == 1)
    {
        if (pulOutVersion)
        {
            *pulOutVersion = NET_ANDROID_MODULE_VERSION;
        }
        return 1;
    }

    // Normal compatibility check - return success if expected version matches
    // pulOutVersion is NOT a valid pointer here (it's MTASA_VERSION_TYPE = 9)
    if (ulExpectedVersion == NET_ANDROID_MODULE_VERSION)
    {
        return 1;
    }

    // Version mismatch - don't write to pulOutVersion (it's not a valid pointer)
    return 0;
}

MTAEXPORT CNetServer* InitNetServerInterface()
{
    if (!g_pNetServer)
    {
        g_pNetServer = new CNetServerAndroid();
    }
    return g_pNetServer;
}

MTAEXPORT void ReleaseNetServerInterface()
{
    if (g_pNetServer)
    {
        delete g_pNetServer;
        g_pNetServer = nullptr;
    }
}
