/*****************************************************************************
 *
 *  PROJECT:     MTA:SA Server - Android Network Module
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        Server/net-android/CNetServerAndroid.h
 *  PURPOSE:     Network server for Android clients (no Anti-Cheat)
 *
 *  This module allows Android clients to connect to MTA servers without
 *  the Anti-Cheat validation that blocks non-Windows clients.
 *
 *  Architecture:
 *    Port 22003 -> net.dll     (PC clients with AC)
 *    Port 22010 -> net_android (Android clients, no AC)
 *              \      |
 *               \     v
 *                -> deathmatch mod (same game logic)
 *
 *****************************************************************************/

#pragma once

#include "CNetBitStreamAndroid.h"

#include <map>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <string>
#include <cstdint>
#include <cstring>

// Forward declarations
class CNetBitStreamAndroid;

//=============================================================================
// Shared types (matching Server/sdk/net)
//=============================================================================

template <int MAX_LENGTH>
class SFixedString
{
public:
    SFixedString() { m_data[0] = '\0'; }
    SFixedString(const char* str) { *this = str; }

    operator const char*() const { return m_data; }

    SFixedString& operator=(const char* str)
    {
        if (str)
        {
            strncpy(m_data, str, MAX_LENGTH - 1);
            m_data[MAX_LENGTH - 1] = '\0';
        }
        else
        {
            m_data[0] = '\0';
        }
        return *this;
    }

private:
    char m_data[MAX_LENGTH];
};

struct SNetExtraInfo
{
    bool m_bHasPing = false;
    unsigned int m_uiPing = 0;
};

struct SPacketStat
{
    int iCount;
    int iTotalBytes;
    long long totalTime;
};

struct NetRawStatistics
{
    unsigned messageSendBuffer[4];
    unsigned messagesSent[4];
    long long messageDataBitsSent[4];
    long long messageTotalBitsSent[4];
    unsigned packetsContainingOnlyAcknowlegements;
    unsigned acknowlegementsSent;
    unsigned acknowlegementsPending;
    long long acknowlegementBitsSent;
    unsigned packetsContainingOnlyAcknowlegementsAndResends;
    unsigned messageResends;
    long long messageDataBitsResent;
    long long messagesTotalBitsResent;
    unsigned messagesOnResendQueue;
    unsigned numberOfUnsplitMessages;
    unsigned numberOfSplitMessages;
    unsigned totalSplits;
    unsigned packetsSent;
    long long encryptionBitsSent;
    long long totalBitsSent;
    unsigned sequencedMessagesOutOfOrder;
    unsigned sequencedMessagesInOrder;
    unsigned orderedMessagesOutOfOrder;
    unsigned orderedMessagesInOrder;
    unsigned packetsReceived;
    unsigned packetsWithBadCRCReceived;
    long long bitsReceived;
    long long bitsWithBadCRCReceived;
    unsigned acknowlegementsReceived;
    unsigned duplicateAcknowlegementsReceived;
    unsigned messagesReceived;
    unsigned invalidMessagesReceived;
    unsigned duplicateMessagesReceived;
    unsigned messagesWaitingForReassembly;
    unsigned internalOutputQueueSize;
    double bitsPerSecond;
    long long connectionStartTime;
    bool bandwidthExceeded;
};

struct NetStatistics
{
    unsigned long long bytesReceived;
    unsigned long long bytesSent;
    unsigned int packetsReceived;
    unsigned int packetsSent;
    float packetlossTotal;
    float packetlossLastSecond;
    unsigned int messagesInSendBuffer;
    unsigned int messagesInResendBuffer;
    bool isLimitedByCongestionControl;
    bool isLimitedByOutgoingBandwidthLimit;
    NetRawStatistics raw;
};

struct SThreadCPUTimes
{
    long long user;
    long long kernel;
};

struct SBandwidthStatistics
{
    long long llOutgoingUDPByteCount;
    long long llIncomingUDPByteCount;
    long long llIncomingUDPByteCountBlocked;
    long long llOutgoingUDPPacketCount;
    long long llIncomingUDPPacketCount;
    long long llIncomingUDPPacketCountBlocked;
    long long llOutgoingUDPByteResentCount;
    long long llOutgoingUDPMessageResentCount;
    SThreadCPUTimes threadCPUTimes;
};

struct SNetPerformanceStatistics
{
    unsigned int uiUpdateCycleRecvTimeAvgUs;
    unsigned int uiUpdateCycleSendTimeAvgUs;
    unsigned int uiUpdateCycleRecvTimeMaxUs;
    unsigned int uiUpdateCycleSendTimeMaxUs;
    float fUpdateCycleRecvDatagramsAvg;
    unsigned int uiUpdateCycleRecvDatagramsMax;
    float fUpdateCycleDatagramsAvg;
    unsigned int uiUpdateCycleDatagramsMax;
    unsigned int uiUpdateCycleDatagramsLimit;
    float fUpdateCycleMessagesAvg;
    unsigned int uiUpdateCycleMessagesMax;
    unsigned int uiUpdateCycleMessagesLimit;
    unsigned int uiUpdateCycleSendsLimitedTotal;
    float fUpdateCycleSendsLimitedPercent;
};

struct SSyncThreadStatistics
{
    unsigned int uiRecvTimeAvgUs;
    unsigned int uiSendTimeAvgUs;
    unsigned int uiRecvTimeMaxUs;
    unsigned int uiSendTimeMaxUs;
    float fRecvMsgsAvg;
    unsigned int uiRecvMsgsMax;
    float fSendCmdsAvg;
    unsigned int uiSendCmdsMax;
};

struct SNetOptions
{
    SNetOptions() { memset(this, 0, sizeof(*this)); }

    struct
    {
        bool bValid;
        int iPacketLoss;
        int iExtraPing;
        int iExtraPingVariance;
        int iKBPSLimit;
    } netSim;

    struct
    {
        bool bValid;
        bool bAutoFilter;
    } netFilter;

    struct
    {
        bool bValid;
        int iUpdateCycleDatagramsLimit;
        int iUpdateCycleMessagesLimit;
    } netOptimize;
};

class NetServerPlayerID
{
protected:
    unsigned long m_uiBinaryAddress;
    unsigned short m_usPort;

public:
    NetServerPlayerID()
    {
        m_uiBinaryAddress = 0xFFFFFFFF;
        m_usPort = 0xFFFF;
    }

    NetServerPlayerID(unsigned long uiBinaryAddress, unsigned short usPort)
    {
        m_uiBinaryAddress = uiBinaryAddress;
        m_usPort = usPort;
    }

    friend inline int operator==(const NetServerPlayerID& left, const NetServerPlayerID& right)
    {
        return left.m_uiBinaryAddress == right.m_uiBinaryAddress && left.m_usPort == right.m_usPort;
    }

    friend inline int operator!=(const NetServerPlayerID& left, const NetServerPlayerID& right)
    {
        return ((left.m_uiBinaryAddress != right.m_uiBinaryAddress) || (left.m_usPort != right.m_usPort));
    }

    friend inline bool operator<(const NetServerPlayerID& left, const NetServerPlayerID& right)
    {
        return left.m_uiBinaryAddress < right.m_uiBinaryAddress ||
               (left.m_uiBinaryAddress == right.m_uiBinaryAddress && left.m_usPort < right.m_usPort);
    }

    unsigned long GetBinaryAddress() const { return m_uiBinaryAddress; }
    unsigned short GetPort() const { return m_usPort; }
};

enum NetServerPacketPriority
{
    PACKET_PRIORITY_HIGH = 0,
    PACKET_PRIORITY_MEDIUM,
    PACKET_PRIORITY_LOW,
    PACKET_PRIORITY_COUNT
};

enum NetServerPacketReliability
{
    PACKET_RELIABILITY_UNRELIABLE = 0,
    PACKET_RELIABILITY_UNRELIABLE_SEQUENCED,
    PACKET_RELIABILITY_RELIABLE,
    PACKET_RELIABILITY_RELIABLE_ORDERED,
    PACKET_RELIABILITY_RELIABLE_SEQUENCED
};

enum ePacketOrdering
{
    PACKET_ORDERING_DEFAULT = 0,
    PACKET_ORDERING_CHAT,
    PACKET_ORDERING_DATA_TRANSFER,
    PACKET_ORDERING_VOICE,
};

// Download mode (HTTP)
namespace EDownloadMode
{
    enum EDownloadModeType
    {
        NONE,
        ASE,
        CALL_REMOTE,
    };
}
using EDownloadMode::EDownloadModeType;

// HTTP download callback type
struct SHttpDownloadResult
{
    const char* pData;
    size_t      dataSize;
    void*       pObj;
    bool        bSuccess;
    int         iErrorCode;
    const char* szHeaders;
    uint        uiAttemptNumber;
    uint        uiContentLength;
};

struct SDownloadStatus
{
    uint uiAttemptNumber = 0;
    uint uiContentLength = 0;
    uint uiBytesReceived = 0;
};

// HTTP request options (simplified for stub)
struct SHttpRequestOptionsTx
{
    bool bIsLegacy = false;
    bool bIsLocal = false;
    bool bCheckContents = false;
    bool bResumeFile = false;
    bool bPostBinary = false;
    uint uiConnectionAttempts = 10;
    uint uiConnectTimeoutMs = 10000;
    uint uiMaxRedirects = 8;
};

typedef void (*PFN_DOWNLOAD_FINISHED_CALLBACK)(const SHttpDownloadResult& result);

class CNetHTTPDownloadManagerInterface
{
public:
    virtual ~CNetHTTPDownloadManagerInterface() {}
    virtual uint GetDownloadSizeNow() = 0;
    virtual void ResetDownloadSize() = 0;
    virtual const char* GetError() = 0;
    virtual bool ProcessQueuedFiles() = 0;
    virtual bool QueueFile(const char* szURL, const char* szOutputFile, void* objectPtr = nullptr,
                           PFN_DOWNLOAD_FINISHED_CALLBACK pfnDownloadFinishedCallback = nullptr,
                           const SHttpRequestOptionsTx& options = SHttpRequestOptionsTx()) = 0;
    virtual void SetMaxConnections(int iMaxConnections) = 0;
    virtual void Reset() = 0;
    virtual bool CancelDownload(void* objectPtr, PFN_DOWNLOAD_FINISHED_CALLBACK pfnDownloadFinishedCallback) = 0;
    virtual bool GetDownloadStatus(void* objectPtr, PFN_DOWNLOAD_FINISHED_CALLBACK pfnDownloadFinishedCallback, SDownloadStatus& outDownloadStatus) = 0;
};

// Stub HTTP download manager (no-op implementation)
class CNetHTTPDownloadManagerStub : public CNetHTTPDownloadManagerInterface
{
public:
    uint GetDownloadSizeNow() override { return 0; }
    void ResetDownloadSize() override {}
    const char* GetError() override { return ""; }
    bool ProcessQueuedFiles() override { return true; }
    bool QueueFile(const char*, const char*, void*, PFN_DOWNLOAD_FINISHED_CALLBACK, const SHttpRequestOptionsTx&) override { return true; }
    void SetMaxConnections(int) override {}
    void Reset() override {}
    bool CancelDownload(void*, PFN_DOWNLOAD_FINISHED_CALLBACK) override { return false; }
    bool GetDownloadStatus(void*, PFN_DOWNLOAD_FINISHED_CALLBACK, SDownloadStatus& outStatus) override { outStatus = SDownloadStatus(); return false; }
};

// Packet handler callback type
typedef bool (*PPACKETHANDLER)(unsigned char, const NetServerPlayerID&, NetBitStreamInterface*, SNetExtraInfo*);

//=============================================================================
// RakNet 3.x Protocol Constants (same as MTA client)
//=============================================================================

namespace RakNetPacketID
{
    static const uint8_t INTERNAL_PING = 0x00;
    static const uint8_t PING = 0x01;
    static const uint8_t PING_OPEN_CONNECTIONS = 0x02;
    static const uint8_t CONNECTED_PONG = 0x03;
    static const uint8_t CONNECTION_REQUEST = 0x04;
    static const uint8_t SECURED_CONNECTION_RESPONSE = 0x05;
    static const uint8_t SECURED_CONNECTION_CONFIRMATION = 0x06;
    static const uint8_t RPC_MAPPING = 0x07;
    static const uint8_t DETECT_LOST_CONNECTIONS = 0x08;
    static const uint8_t OPEN_CONNECTION_REQUEST = 0x09;
    static const uint8_t OPEN_CONNECTION_REPLY = 0x0A;
    static const uint8_t RPC = 0x0B;
    static const uint8_t RPC_REPLY = 0x0C;
    static const uint8_t OUT_OF_BAND_INTERNAL = 0x0D;
    static const uint8_t CONNECTION_REQUEST_ACCEPTED = 0x0E;
    static const uint8_t CONNECTION_ATTEMPT_FAILED = 0x0F;
    static const uint8_t ALREADY_CONNECTED = 0x10;
    static const uint8_t NEW_INCOMING_CONNECTION = 0x11;
    static const uint8_t NO_FREE_INCOMING_CONNECTIONS = 0x12;
    static const uint8_t DISCONNECTION_NOTIFICATION = 0x13;
    static const uint8_t CONNECTION_LOST = 0x14;
    static const uint8_t RSA_PUBLIC_KEY_MISMATCH = 0x15;
    static const uint8_t CONNECTION_BANNED = 0x16;
    static const uint8_t INVALID_PASSWORD = 0x17;
    static const uint8_t INCOMPATIBLE_PROTOCOL_VERSION = 0x44;
}

// MTA packet IDs (after RakNet handshake)
namespace MTAPacketID
{
    static const uint8_t MOD_NAME = 0x1C;  // Server -> Client
    static const uint8_t PLAYER_JOINDATA = 0x01;  // Client -> Server
    static const uint8_t SERVER_JOIN_COMPLETE = 0x02;  // Server -> Client
    static const uint8_t SERVER_JOINEDGAME = 0x16;  // Server -> Client (22)
}

//=============================================================================
// Client Connection State
//=============================================================================

enum class ClientState
{
    DISCONNECTED,
    HANDSHAKE_OPEN_REQUEST,  // Received 0x09, need to send 0x0A
    HANDSHAKE_CONNECTION,    // Received 0x04, need to send 0x0E
    AWAITING_JOINDATA,       // Sent MOD_NAME, waiting for JOINDATA
    CONNECTED,               // Fully connected
    DISCONNECTING
};

struct ClientConnection
{
    NetServerPlayerID   playerID;
    ClientState         state = ClientState::DISCONNECTED;
    uint32_t            cookie = 0;
    uint64_t            guid = 0;
    uint64_t            connectTime = 0;
    uint64_t            lastPacketTime = 0;
    uint16_t            bitstreamVersion = 0x06B;  // Latest MTA version
    std::string         playerName;

    // Statistics
    uint64_t            packetsReceived = 0;
    uint64_t            packetsSent = 0;
    uint64_t            bytesReceived = 0;
    uint64_t            bytesSent = 0;
};

//=============================================================================
// CNetServer - Base Interface (matching Server/sdk/net/CNetServer.h)
//=============================================================================

class CNetServer
{
public:
    enum ENetworkUsageDirection
    {
        STATS_INCOMING_TRAFFIC = 0,
        STATS_OUTGOING_TRAFFIC = 1
    };

    // Network methods - MUST match exact order from Server/sdk/net/CNetServer.h
    virtual bool StartNetwork(const char* szIP, unsigned short usServerPort,
                             unsigned int uiAllowedPlayers, const char* szServerName) = 0;
    virtual void StopNetwork() = 0;
    virtual void DoPulse() = 0;
    virtual void RegisterPacketHandler(PPACKETHANDLER pfnPacketHandler) = 0;
    virtual bool GetNetworkStatistics(NetStatistics* pDest, const NetServerPlayerID& PlayerID) = 0;
    virtual const SPacketStat* GetPacketStats() = 0;
    virtual bool GetBandwidthStatistics(SBandwidthStatistics* pDest) = 0;
    virtual bool GetNetPerformanceStatistics(SNetPerformanceStatistics* pDest, bool bResetCounters) = 0;
    virtual void GetPingStatus(SFixedString<32>* pstrStatus) = 0;
    virtual bool GetSyncThreadStatistics(SSyncThreadStatistics* pDest, bool bResetCounters) = 0;
    virtual NetBitStreamInterface* AllocateNetServerBitStream(unsigned short usBitStreamVersion,
                                                               const void* pData = nullptr,
                                                               unsigned int uiDataSize = 0,
                                                               bool bCopyData = false) = 0;
    virtual void DeallocateNetServerBitStream(NetBitStreamInterface* bitStream) = 0;
    virtual bool SendPacket(unsigned char ucPacketID, const NetServerPlayerID& playerID,
                           NetBitStreamInterface* bitStream, bool bBroadcast,
                           NetServerPacketPriority packetPriority,
                           NetServerPacketReliability packetReliability,
                           ePacketOrdering packetOrdering = PACKET_ORDERING_DEFAULT) = 0;
    virtual void GetPlayerIP(const NetServerPlayerID& playerID, char strIP[22], unsigned short* usPort) = 0;
    virtual void Kick(const NetServerPlayerID& PlayerID) = 0;
    virtual void SetPassword(const char* szPassword) = 0;
    virtual void SetMaximumIncomingConnections(unsigned short numberAllowed) = 0;
    virtual CNetHTTPDownloadManagerInterface* GetHTTPDownloadManager(EDownloadModeType iMode) = 0;
    virtual void SetClientBitStreamVersion(const NetServerPlayerID& PlayerID, unsigned short usBitStreamVersion) = 0;
    virtual void ClearClientBitStreamVersion(const NetServerPlayerID& PlayerID) = 0;
    virtual void SetChecks(const char* szDisableComboACMap, const char* szDisableACMap,
                          const char* szEnableSDMap, int iEnableClientChecks,
                          bool bHideAC, const char* szImgMods) = 0;
    virtual unsigned int GetPendingPacketCount() = 0;
    virtual void GetNetRoute(SFixedString<32>* pstrRoute) = 0;
    virtual bool InitServerId(const char* szPath) = 0;
    virtual void ResendModPackets(const NetServerPlayerID& playerID) = 0;
    virtual void ResendACPackets(const NetServerPlayerID& playerID) = 0;
    virtual void GetClientSerialAndVersion(const NetServerPlayerID& playerID,
                                           SFixedString<32>& strSerial,
                                           SFixedString<64>& strExtra,
                                           SFixedString<32>& strVersion) = 0;
    virtual void SetNetOptions(const SNetOptions& options) = 0;
    virtual void GenerateRandomData(void* pOutData, unsigned int uiLength) = 0;

    // Methods with default implementations in original header (still in vtable!)
    virtual bool EncryptDumpfile(const char* szClearPathFilename, const char* szEncryptedPathFilename) { return false; }
    virtual bool ValidateHttpCacheFileName(const char* szFilename) { return false; }
    virtual bool GetScriptInfo(const char* cpInBuffer, uint uiInSize, void* pOutInfo) { return false; }
    virtual bool DeobfuscateScript(const char* cpInBuffer, uint uiInSize, const char** pcpOutBuffer, uint* puiOutSize, const char* szScriptName) { return false; }
    virtual bool GetPlayerPacketUsageStats(unsigned char* packetIdList, uint uiNumPacketIds, void* pOutStats, uint uiTopCount) { return false; }
    virtual const char* GetLogOutput() { return nullptr; }
    virtual bool IsValidSocket(const NetServerPlayerID& playerID) { return false; }
};

//=============================================================================
// CNetServerAndroid - Main Server Class
//=============================================================================

class CNetServerAndroid : public CNetServer
{
public:
    CNetServerAndroid();
    virtual ~CNetServerAndroid();

    //=========================================================================
    // CNetServer Interface Implementation
    //=========================================================================

    // Network methods
    virtual bool StartNetwork(const char* szIP, unsigned short usServerPort,
                             unsigned int uiAllowedPlayers, const char* szServerName) override;
    virtual void StopNetwork() override;
    virtual void DoPulse() override;
    virtual void RegisterPacketHandler(PPACKETHANDLER pfnPacketHandler) override;
    virtual bool GetNetworkStatistics(NetStatistics* pDest, const NetServerPlayerID& PlayerID) override;
    virtual const SPacketStat* GetPacketStats() override;
    virtual bool GetBandwidthStatistics(SBandwidthStatistics* pDest) override;
    virtual bool GetNetPerformanceStatistics(SNetPerformanceStatistics* pDest, bool bResetCounters) override;
    virtual void GetPingStatus(SFixedString<32>* pstrStatus) override;
    virtual bool GetSyncThreadStatistics(SSyncThreadStatistics* pDest, bool bResetCounters) override;
    virtual NetBitStreamInterface* AllocateNetServerBitStream(unsigned short usBitStreamVersion,
                                                               const void* pData = nullptr,
                                                               unsigned int uiDataSize = 0,
                                                               bool bCopyData = false) override;
    virtual void DeallocateNetServerBitStream(NetBitStreamInterface* bitStream) override;
    virtual bool SendPacket(unsigned char ucPacketID, const NetServerPlayerID& playerID,
                           NetBitStreamInterface* bitStream, bool bBroadcast,
                           NetServerPacketPriority packetPriority,
                           NetServerPacketReliability packetReliability,
                           ePacketOrdering packetOrdering = PACKET_ORDERING_DEFAULT) override;
    virtual void GetPlayerIP(const NetServerPlayerID& playerID, char strIP[22], unsigned short* usPort) override;
    virtual void Kick(const NetServerPlayerID& PlayerID) override;
    virtual void SetPassword(const char* szPassword) override;
    virtual void SetMaximumIncomingConnections(unsigned short numberAllowed) override;
    virtual CNetHTTPDownloadManagerInterface* GetHTTPDownloadManager(EDownloadModeType iMode) override;
    virtual void SetClientBitStreamVersion(const NetServerPlayerID& PlayerID, unsigned short usBitStreamVersion) override;
    virtual void ClearClientBitStreamVersion(const NetServerPlayerID& PlayerID) override;
    virtual void SetChecks(const char* szDisableComboACMap, const char* szDisableACMap,
                          const char* szEnableSDMap, int iEnableClientChecks,
                          bool bHideAC, const char* szImgMods) override;
    virtual unsigned int GetPendingPacketCount() override;
    virtual void GetNetRoute(SFixedString<32>* pstrRoute) override;
    virtual bool InitServerId(const char* szPath) override;
    virtual void ResendModPackets(const NetServerPlayerID& playerID) override;
    virtual void ResendACPackets(const NetServerPlayerID& playerID) override;
    virtual void GetClientSerialAndVersion(const NetServerPlayerID& playerID,
                                           SFixedString<32>& strSerial,
                                           SFixedString<64>& strExtra,
                                           SFixedString<32>& strVersion) override;
    virtual void SetNetOptions(const SNetOptions& options) override;
    virtual void GenerateRandomData(void* pOutData, unsigned int uiLength) override;
    virtual bool IsValidSocket(const NetServerPlayerID& playerID) override;

private:
    //=========================================================================
    // Internal Methods
    //=========================================================================

    void NetworkThreadFunc();
    void ProcessIncomingPacket(const uint8_t* data, int length,
                               const struct sockaddr_in& fromAddr);
    void HandleOpenConnectionRequest(const uint8_t* data, int length,
                                     const struct sockaddr_in& clientAddr);
    void HandleConnectionRequest(const uint8_t* data, int length,
                                 ClientConnection& client);
    void HandleNewIncomingConnection(ClientConnection& client);
    void SendModName(const ClientConnection& client);
    void HandlePlayerJoinData(const uint8_t* data, int length,
                              ClientConnection& client);
    void SendJoinComplete(ClientConnection& client);
    ClientConnection* GetClient(const NetServerPlayerID& playerID);
    ClientConnection* GetOrCreateClient(const struct sockaddr_in& addr);
    void RemoveClient(const NetServerPlayerID& playerID);
    void CheckClientTimeouts(uint64_t now, uint64_t timeoutMs);
    NetServerPlayerID MakePlayerID(const struct sockaddr_in& addr);
    bool SendRawPacket(const uint8_t* data, int length, const struct sockaddr_in& addr);
    uint64_t GetTimeMs();
    void LogPacket(const char* direction, const uint8_t* data, int length,
                   const struct sockaddr_in& addr);

private:
    //=========================================================================
    // Member Variables
    //=========================================================================

    int                     m_socket = -1;
    unsigned short          m_usPort = 0;
    std::string             m_strServerIP;
    std::string             m_strServerName;
    std::string             m_strPassword;
    unsigned int            m_uiMaxPlayers = 32;

    std::thread             m_networkThread;
    std::atomic<bool>       m_bRunning{false};

    std::map<uint64_t, ClientConnection> m_clients;
    std::mutex              m_clientsMutex;

    PPACKETHANDLER          m_pfnPacketHandler = nullptr;

    SPacketStat             m_packetStats[256];
    uint64_t                m_startTime = 0;
    uint64_t                m_serverGUID = 0;

    static const uint16_t BITSTREAM_VERSION = 0x06B;
};

//=============================================================================
// Module exports
//=============================================================================

extern "C"
{
    CNetServer* InitNetServerInterface();
    void ReleaseNetServerInterface();
}
