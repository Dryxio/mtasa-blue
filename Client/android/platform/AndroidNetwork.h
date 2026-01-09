/*
 * MTA:SA Android - Network Layer
 *
 * Provides network functionality for Android:
 *   - TCP/UDP socket management
 *   - HTTP client for downloads
 *   - Network state monitoring
 *   - Server browser communication
 *
 * Design:
 *   - Uses standard POSIX sockets (available on Android)
 *   - Monitors network connectivity via Android APIs
 *   - Handles Android's network permission requirements
 *   - Thread-safe operation
 */

#ifndef ANDROID_NETWORK_H
#define ANDROID_NETWORK_H

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <atomic>
#include <thread>

#ifdef __ANDROID__
#include <jni.h>
#endif

namespace MTA::Android::Platform
{

//=============================================================================
// Network Types
//=============================================================================

enum class NetworkType
{
    None,
    WiFi,
    Mobile,
    Ethernet,
    Unknown
};

enum class ConnectionState
{
    Disconnected,
    Connecting,
    Connected,
    Disconnecting
};

//=============================================================================
// Socket Wrapper
//=============================================================================

class Socket
{
public:
    enum class Type { TCP, UDP };
    enum class State { Closed, Connecting, Connected, Listening, Error };

    Socket(Type type);
    ~Socket();

    // Connection
    bool Connect(const std::string& host, uint16_t port);
    bool Bind(uint16_t port);
    bool Listen(int backlog = 5);
    Socket* Accept();
    void Close();

    // Data transfer
    int Send(const void* data, int size);
    int Receive(void* buffer, int maxSize);

    // UDP specific
    int SendTo(const void* data, int size, const std::string& host, uint16_t port);
    int ReceiveFrom(void* buffer, int maxSize, std::string& outHost, uint16_t& outPort);

    // State
    State GetState() const { return m_state; }
    bool IsConnected() const { return m_state == State::Connected; }
    int GetLastError() const { return m_lastError; }
    std::string GetLastErrorString() const;

    // Configuration
    void SetBlocking(bool blocking);
    void SetTimeout(int timeoutMs);
    void SetReuseAddress(bool reuse);
    void SetNoDelay(bool noDelay);  // TCP only

    // Info
    int GetSocket() const { return m_socket; }
    std::string GetLocalAddress() const;
    uint16_t GetLocalPort() const;
    std::string GetRemoteAddress() const;
    uint16_t GetRemotePort() const;

private:
    Type m_type;
    State m_state;
    int m_socket;
    int m_lastError;
    bool m_blocking;
};

//=============================================================================
// HTTP Client
//=============================================================================

struct HttpResponse
{
    int statusCode;
    std::string statusText;
    std::vector<std::pair<std::string, std::string>> headers;
    std::vector<uint8_t> body;
    std::string error;
    bool success;
};

using HttpCallback = std::function<void(const HttpResponse&)>;
using ProgressCallback = std::function<void(int64_t downloaded, int64_t total)>;

class HttpClient
{
public:
    HttpClient();
    ~HttpClient();

    // Synchronous requests
    HttpResponse Get(const std::string& url);
    HttpResponse Post(const std::string& url, const std::string& body,
                      const std::string& contentType = "application/x-www-form-urlencoded");

    // Asynchronous requests
    void GetAsync(const std::string& url, HttpCallback callback);
    void PostAsync(const std::string& url, const std::string& body,
                   HttpCallback callback,
                   const std::string& contentType = "application/x-www-form-urlencoded");

    // Download file
    bool DownloadFile(const std::string& url, const std::string& outputPath,
                      ProgressCallback progress = nullptr);
    void DownloadFileAsync(const std::string& url, const std::string& outputPath,
                           HttpCallback callback, ProgressCallback progress = nullptr);

    // Configuration
    void SetTimeout(int timeoutMs) { m_timeout = timeoutMs; }
    void SetUserAgent(const std::string& userAgent) { m_userAgent = userAgent; }
    void AddHeader(const std::string& name, const std::string& value);
    void ClearHeaders();

    // Cancel pending requests
    void Cancel();

private:
    HttpResponse DoRequest(const std::string& method, const std::string& url,
                          const std::string& body, const std::string& contentType);
    bool ParseUrl(const std::string& url, std::string& host, uint16_t& port,
                  std::string& path, bool& isHttps);

private:
    int m_timeout;
    std::string m_userAgent;
    std::vector<std::pair<std::string, std::string>> m_headers;
    std::atomic<bool> m_cancelled;
};

//=============================================================================
// Server Entry (for server browser)
//=============================================================================

struct ServerInfo
{
    std::string address;
    uint16_t port;
    std::string name;
    std::string gameMode;
    std::string map;
    int players;
    int maxPlayers;
    bool passworded;
    int ping;
    std::string version;
};

//=============================================================================
// Android Network Manager
//=============================================================================

class AndroidNetwork
{
public:
    static AndroidNetwork& Instance();

    //=========================================================================
    // Initialization
    //=========================================================================

#ifdef __ANDROID__
    bool Initialize(JNIEnv* env, jobject context);
#else
    bool Initialize();
#endif
    void Shutdown();

    //=========================================================================
    // Network State
    //=========================================================================

    /**
     * Check if network is available
     */
    bool IsNetworkAvailable() const { return m_networkAvailable; }

    /**
     * Get current network type
     */
    NetworkType GetNetworkType() const { return m_networkType; }

    /**
     * Get network type as string
     */
    std::string GetNetworkTypeString() const;

    /**
     * Check for metered connection (mobile data)
     */
    bool IsMeteredConnection() const { return m_isMetered; }

    /**
     * Update network state (called from Java)
     */
    void OnNetworkStateChanged(bool available, int type, bool metered);

    //=========================================================================
    // DNS Resolution
    //=========================================================================

    /**
     * Resolve hostname to IP address
     */
    std::string ResolveHostname(const std::string& hostname);

    /**
     * Resolve hostname to list of IP addresses
     */
    std::vector<std::string> ResolveHostnameAll(const std::string& hostname);

    //=========================================================================
    // Server Browser
    //=========================================================================

    /**
     * Query master server for server list
     */
    std::vector<ServerInfo> QueryMasterServer(const std::string& masterUrl);

    /**
     * Query individual server for info
     */
    bool QueryServerInfo(const std::string& address, uint16_t port, ServerInfo& outInfo);

    /**
     * Ping a server
     */
    int PingServer(const std::string& address, uint16_t port);

    //=========================================================================
    // Utilities
    //=========================================================================

    /**
     * Get local IP address
     */
    std::string GetLocalIPAddress();

    /**
     * Check if address is local/LAN
     */
    bool IsLocalAddress(const std::string& address);

    /**
     * Get MAC address (if available)
     */
    std::string GetMACAddress();

    //=========================================================================
    // Callbacks
    //=========================================================================

    using NetworkStateCallback = std::function<void(bool available, NetworkType type)>;
    void SetNetworkStateCallback(NetworkStateCallback callback)
    {
        m_stateCallback = callback;
    }

private:
    AndroidNetwork();
    ~AndroidNetwork();
    AndroidNetwork(const AndroidNetwork&) = delete;
    AndroidNetwork& operator=(const AndroidNetwork&) = delete;

    void UpdateNetworkState();

private:
    bool m_initialized;

    // Network state
    std::atomic<bool> m_networkAvailable;
    NetworkType m_networkType;
    bool m_isMetered;

    // Callback
    NetworkStateCallback m_stateCallback;

    // Thread safety
    mutable std::mutex m_mutex;

#ifdef __ANDROID__
    // JNI references
    jobject m_connectivityManager;
    jmethodID m_getActiveNetworkInfo;
#endif
};

//=============================================================================
// Inline Implementations
//=============================================================================

inline AndroidNetwork& AndroidNetwork::Instance()
{
    static AndroidNetwork instance;
    return instance;
}

inline std::string AndroidNetwork::GetNetworkTypeString() const
{
    switch (m_networkType)
    {
        case NetworkType::WiFi:     return "WiFi";
        case NetworkType::Mobile:   return "Mobile";
        case NetworkType::Ethernet: return "Ethernet";
        case NetworkType::None:     return "None";
        default:                    return "Unknown";
    }
}

//=============================================================================
// MTA Server Protocol Constants
//=============================================================================

namespace MTAProtocol
{
    // Server query ports
    constexpr uint16_t DEFAULT_PORT = 22003;
    constexpr uint16_t ASE_PORT_OFFSET = 123;  // ASE query port = game port + 123

    // Master server
    constexpr const char* MASTER_SERVER_URL = "https://master.mtasa.com/";
    constexpr const char* MASTER_SERVER_LIST = "https://master.mtasa.com/ase/mta/";

    // Protocol versions
    constexpr uint16_t PROTOCOL_VERSION = 0x0166;  // 1.6.x

    // Packet types
    constexpr uint8_t PACKET_QUERY = 's';
    constexpr uint8_t PACKET_RULES = 'r';
    constexpr uint8_t PACKET_PLAYERS = 'c';
}

} // namespace MTA::Android::Platform

#endif // ANDROID_NETWORK_H
