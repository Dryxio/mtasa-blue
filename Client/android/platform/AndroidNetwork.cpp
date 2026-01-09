/*
 * MTA:SA Android - Network Layer Implementation
 */

#include "AndroidNetwork.h"
#include <cstring>
#include <algorithm>
#include <chrono>

// POSIX socket headers
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#ifdef __ANDROID__
#include <android/log.h>
#define LOG_TAG "MTA-Network"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#include <cstdio>
#define LOGI(...) printf(__VA_ARGS__)
#define LOGD(...) printf(__VA_ARGS__)
#define LOGE(...) fprintf(stderr, __VA_ARGS__)
#endif

namespace MTA::Android::Platform
{

//=============================================================================
// Socket Implementation
//=============================================================================

Socket::Socket(Type type)
    : m_type(type)
    , m_state(State::Closed)
    , m_socket(-1)
    , m_lastError(0)
    , m_blocking(true)
{
    int sockType = (type == Type::TCP) ? SOCK_STREAM : SOCK_DGRAM;
    int protocol = (type == Type::TCP) ? IPPROTO_TCP : IPPROTO_UDP;

    m_socket = socket(AF_INET, sockType, protocol);
    if (m_socket < 0)
    {
        m_lastError = errno;
        m_state = State::Error;
        LOGE("Failed to create socket: %s", strerror(errno));
    }
}

Socket::~Socket()
{
    Close();
}

bool Socket::Connect(const std::string& host, uint16_t port)
{
    if (m_socket < 0)
        return false;

    // Resolve hostname
    struct hostent* he = gethostbyname(host.c_str());
    if (!he)
    {
        m_lastError = h_errno;
        m_state = State::Error;
        LOGE("Failed to resolve host: %s", host.c_str());
        return false;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

    m_state = State::Connecting;

    int result = connect(m_socket, (struct sockaddr*)&addr, sizeof(addr));
    if (result < 0)
    {
        if (errno == EINPROGRESS && !m_blocking)
        {
            // Non-blocking connect in progress
            return true;
        }

        m_lastError = errno;
        m_state = State::Error;
        LOGE("Failed to connect to %s:%d: %s", host.c_str(), port, strerror(errno));
        return false;
    }

    m_state = State::Connected;
    LOGD("Connected to %s:%d", host.c_str(), port);
    return true;
}

bool Socket::Bind(uint16_t port)
{
    if (m_socket < 0)
        return false;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    int result = bind(m_socket, (struct sockaddr*)&addr, sizeof(addr));
    if (result < 0)
    {
        m_lastError = errno;
        m_state = State::Error;
        LOGE("Failed to bind to port %d: %s", port, strerror(errno));
        return false;
    }

    LOGD("Bound to port %d", port);
    return true;
}

bool Socket::Listen(int backlog)
{
    if (m_socket < 0 || m_type != Type::TCP)
        return false;

    int result = listen(m_socket, backlog);
    if (result < 0)
    {
        m_lastError = errno;
        m_state = State::Error;
        LOGE("Failed to listen: %s", strerror(errno));
        return false;
    }

    m_state = State::Listening;
    return true;
}

Socket* Socket::Accept()
{
    if (m_socket < 0 || m_state != State::Listening)
        return nullptr;

    struct sockaddr_in clientAddr;
    socklen_t addrLen = sizeof(clientAddr);

    int clientSocket = accept(m_socket, (struct sockaddr*)&clientAddr, &addrLen);
    if (clientSocket < 0)
    {
        if (errno != EWOULDBLOCK && errno != EAGAIN)
        {
            m_lastError = errno;
            LOGE("Failed to accept: %s", strerror(errno));
        }
        return nullptr;
    }

    Socket* client = new Socket(Type::TCP);
    close(client->m_socket);  // Close the auto-created socket
    client->m_socket = clientSocket;
    client->m_state = State::Connected;
    client->m_blocking = m_blocking;

    char clientIP[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
    LOGD("Accepted connection from %s:%d", clientIP, ntohs(clientAddr.sin_port));

    return client;
}

void Socket::Close()
{
    if (m_socket >= 0)
    {
        close(m_socket);
        m_socket = -1;
    }
    m_state = State::Closed;
}

int Socket::Send(const void* data, int size)
{
    if (m_socket < 0 || m_state != State::Connected)
        return -1;

    int sent = send(m_socket, data, size, MSG_NOSIGNAL);
    if (sent < 0)
    {
        m_lastError = errno;
        if (errno != EWOULDBLOCK && errno != EAGAIN)
        {
            LOGE("Send failed: %s", strerror(errno));
        }
    }
    return sent;
}

int Socket::Receive(void* buffer, int maxSize)
{
    if (m_socket < 0)
        return -1;

    int received = recv(m_socket, buffer, maxSize, 0);
    if (received < 0)
    {
        m_lastError = errno;
        if (errno != EWOULDBLOCK && errno != EAGAIN)
        {
            LOGE("Receive failed: %s", strerror(errno));
        }
    }
    else if (received == 0 && m_type == Type::TCP)
    {
        // Connection closed by peer
        m_state = State::Closed;
    }
    return received;
}

int Socket::SendTo(const void* data, int size, const std::string& host, uint16_t port)
{
    if (m_socket < 0)
        return -1;

    struct hostent* he = gethostbyname(host.c_str());
    if (!he)
        return -1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

    return sendto(m_socket, data, size, 0, (struct sockaddr*)&addr, sizeof(addr));
}

int Socket::ReceiveFrom(void* buffer, int maxSize, std::string& outHost, uint16_t& outPort)
{
    if (m_socket < 0)
        return -1;

    struct sockaddr_in addr;
    socklen_t addrLen = sizeof(addr);

    int received = recvfrom(m_socket, buffer, maxSize, 0, (struct sockaddr*)&addr, &addrLen);
    if (received >= 0)
    {
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, ip, INET_ADDRSTRLEN);
        outHost = ip;
        outPort = ntohs(addr.sin_port);
    }

    return received;
}

std::string Socket::GetLastErrorString() const
{
    return strerror(m_lastError);
}

void Socket::SetBlocking(bool blocking)
{
    if (m_socket < 0)
        return;

    int flags = fcntl(m_socket, F_GETFL, 0);
    if (blocking)
        flags &= ~O_NONBLOCK;
    else
        flags |= O_NONBLOCK;

    fcntl(m_socket, F_SETFL, flags);
    m_blocking = blocking;
}

void Socket::SetTimeout(int timeoutMs)
{
    if (m_socket < 0)
        return;

    struct timeval tv;
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;

    setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(m_socket, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

void Socket::SetReuseAddress(bool reuse)
{
    if (m_socket < 0)
        return;

    int opt = reuse ? 1 : 0;
    setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
}

void Socket::SetNoDelay(bool noDelay)
{
    if (m_socket < 0 || m_type != Type::TCP)
        return;

    int opt = noDelay ? 1 : 0;
    setsockopt(m_socket, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
}

std::string Socket::GetLocalAddress() const
{
    if (m_socket < 0)
        return "";

    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    if (getsockname(m_socket, (struct sockaddr*)&addr, &len) < 0)
        return "";

    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, ip, INET_ADDRSTRLEN);
    return ip;
}

uint16_t Socket::GetLocalPort() const
{
    if (m_socket < 0)
        return 0;

    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    if (getsockname(m_socket, (struct sockaddr*)&addr, &len) < 0)
        return 0;

    return ntohs(addr.sin_port);
}

std::string Socket::GetRemoteAddress() const
{
    if (m_socket < 0)
        return "";

    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    if (getpeername(m_socket, (struct sockaddr*)&addr, &len) < 0)
        return "";

    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, ip, INET_ADDRSTRLEN);
    return ip;
}

uint16_t Socket::GetRemotePort() const
{
    if (m_socket < 0)
        return 0;

    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    if (getpeername(m_socket, (struct sockaddr*)&addr, &len) < 0)
        return 0;

    return ntohs(addr.sin_port);
}

//=============================================================================
// HttpClient Implementation
//=============================================================================

HttpClient::HttpClient()
    : m_timeout(30000)
    , m_userAgent("MTA-Android/1.6")
    , m_cancelled(false)
{
}

HttpClient::~HttpClient()
{
    Cancel();
}

bool HttpClient::ParseUrl(const std::string& url, std::string& host, uint16_t& port,
                          std::string& path, bool& isHttps)
{
    std::string remaining = url;

    // Check protocol
    isHttps = false;
    if (remaining.find("https://") == 0)
    {
        isHttps = true;
        remaining = remaining.substr(8);
        port = 443;
    }
    else if (remaining.find("http://") == 0)
    {
        remaining = remaining.substr(7);
        port = 80;
    }
    else
    {
        return false;
    }

    // Find path
    size_t pathStart = remaining.find('/');
    if (pathStart != std::string::npos)
    {
        path = remaining.substr(pathStart);
        remaining = remaining.substr(0, pathStart);
    }
    else
    {
        path = "/";
    }

    // Check for port
    size_t colonPos = remaining.find(':');
    if (colonPos != std::string::npos)
    {
        host = remaining.substr(0, colonPos);
        port = std::stoi(remaining.substr(colonPos + 1));
    }
    else
    {
        host = remaining;
    }

    return !host.empty();
}

HttpResponse HttpClient::DoRequest(const std::string& method, const std::string& url,
                                    const std::string& body, const std::string& contentType)
{
    HttpResponse response;
    response.success = false;
    response.statusCode = 0;

    std::string host, path;
    uint16_t port;
    bool isHttps;

    if (!ParseUrl(url, host, port, path, isHttps))
    {
        response.error = "Invalid URL";
        return response;
    }

    // Note: For HTTPS, would need to use SSL library (OpenSSL, mbedTLS, etc.)
    // This implementation only supports HTTP
    if (isHttps)
    {
        response.error = "HTTPS not implemented (use Android's HttpURLConnection via JNI)";
        return response;
    }

    Socket socket(Socket::Type::TCP);
    socket.SetTimeout(m_timeout);

    if (!socket.Connect(host, port))
    {
        response.error = "Connection failed: " + socket.GetLastErrorString();
        return response;
    }

    // Build request
    std::string request;
    request += method + " " + path + " HTTP/1.1\r\n";
    request += "Host: " + host + "\r\n";
    request += "User-Agent: " + m_userAgent + "\r\n";
    request += "Connection: close\r\n";

    for (const auto& header : m_headers)
    {
        request += header.first + ": " + header.second + "\r\n";
    }

    if (!body.empty())
    {
        request += "Content-Type: " + contentType + "\r\n";
        request += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    }

    request += "\r\n";
    request += body;

    // Send request
    if (socket.Send(request.data(), request.size()) < 0)
    {
        response.error = "Send failed: " + socket.GetLastErrorString();
        return response;
    }

    // Receive response
    std::vector<char> buffer(65536);
    std::string responseData;

    while (!m_cancelled)
    {
        int received = socket.Receive(buffer.data(), buffer.size() - 1);
        if (received <= 0)
            break;

        buffer[received] = 0;
        responseData.append(buffer.data(), received);
    }

    if (m_cancelled)
    {
        response.error = "Cancelled";
        return response;
    }

    // Parse response
    size_t headerEnd = responseData.find("\r\n\r\n");
    if (headerEnd == std::string::npos)
    {
        response.error = "Invalid response";
        return response;
    }

    std::string headerSection = responseData.substr(0, headerEnd);
    std::string bodySection = responseData.substr(headerEnd + 4);

    // Parse status line
    size_t statusEnd = headerSection.find("\r\n");
    if (statusEnd != std::string::npos)
    {
        std::string statusLine = headerSection.substr(0, statusEnd);
        size_t codeStart = statusLine.find(' ');
        if (codeStart != std::string::npos)
        {
            response.statusCode = std::stoi(statusLine.substr(codeStart + 1, 3));
            size_t textStart = statusLine.find(' ', codeStart + 1);
            if (textStart != std::string::npos)
            {
                response.statusText = statusLine.substr(textStart + 1);
            }
        }
    }

    response.body.assign(bodySection.begin(), bodySection.end());
    response.success = (response.statusCode >= 200 && response.statusCode < 300);

    return response;
}

HttpResponse HttpClient::Get(const std::string& url)
{
    return DoRequest("GET", url, "", "");
}

HttpResponse HttpClient::Post(const std::string& url, const std::string& body,
                               const std::string& contentType)
{
    return DoRequest("POST", url, body, contentType);
}

void HttpClient::GetAsync(const std::string& url, HttpCallback callback)
{
    std::thread([this, url, callback]() {
        HttpResponse response = Get(url);
        if (callback)
            callback(response);
    }).detach();
}

void HttpClient::PostAsync(const std::string& url, const std::string& body,
                            HttpCallback callback, const std::string& contentType)
{
    std::thread([this, url, body, contentType, callback]() {
        HttpResponse response = Post(url, body, contentType);
        if (callback)
            callback(response);
    }).detach();
}

bool HttpClient::DownloadFile(const std::string& url, const std::string& outputPath,
                               ProgressCallback progress)
{
    HttpResponse response = Get(url);
    if (!response.success)
        return false;

    FILE* fp = fopen(outputPath.c_str(), "wb");
    if (!fp)
        return false;

    fwrite(response.body.data(), 1, response.body.size(), fp);
    fclose(fp);

    return true;
}

void HttpClient::DownloadFileAsync(const std::string& url, const std::string& outputPath,
                                    HttpCallback callback, ProgressCallback progress)
{
    std::thread([this, url, outputPath, callback, progress]() {
        HttpResponse response;
        response.success = DownloadFile(url, outputPath, progress);
        if (callback)
            callback(response);
    }).detach();
}

void HttpClient::AddHeader(const std::string& name, const std::string& value)
{
    m_headers.push_back({name, value});
}

void HttpClient::ClearHeaders()
{
    m_headers.clear();
}

void HttpClient::Cancel()
{
    m_cancelled = true;
}

//=============================================================================
// AndroidNetwork Implementation
//=============================================================================

AndroidNetwork::AndroidNetwork()
    : m_initialized(false)
    , m_networkAvailable(false)
    , m_networkType(NetworkType::Unknown)
    , m_isMetered(false)
#ifdef __ANDROID__
    , m_connectivityManager(nullptr)
    , m_getActiveNetworkInfo(nullptr)
#endif
{
}

AndroidNetwork::~AndroidNetwork()
{
    Shutdown();
}

#ifdef __ANDROID__
bool AndroidNetwork::Initialize(JNIEnv* env, jobject context)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_initialized)
        return true;

    LOGI("Initializing Android network...");

    // Get ConnectivityManager from context
    jclass contextClass = env->GetObjectClass(context);
    jmethodID getSystemService = env->GetMethodID(contextClass, "getSystemService",
        "(Ljava/lang/String;)Ljava/lang/Object;");

    jstring serviceName = env->NewStringUTF("connectivity");
    jobject connManager = env->CallObjectMethod(context, getSystemService, serviceName);
    env->DeleteLocalRef(serviceName);

    if (connManager)
    {
        m_connectivityManager = env->NewGlobalRef(connManager);
        env->DeleteLocalRef(connManager);

        // Get method ID for getActiveNetworkInfo
        jclass cmClass = env->GetObjectClass(m_connectivityManager);
        m_getActiveNetworkInfo = env->GetMethodID(cmClass, "getActiveNetworkInfo",
            "()Landroid/net/NetworkInfo;");
    }

    // Initial network state check
    UpdateNetworkState();

    m_initialized = true;
    LOGI("Android network initialized, available=%d, type=%s",
         m_networkAvailable.load(), GetNetworkTypeString().c_str());

    return true;
}
#else
bool AndroidNetwork::Initialize()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_initialized)
        return true;

    m_networkAvailable = true;
    m_networkType = NetworkType::Unknown;
    m_initialized = true;

    return true;
}
#endif

void AndroidNetwork::Shutdown()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized)
        return;

    LOGI("Shutting down Android network");

#ifdef __ANDROID__
    // Clean up JNI references would require JNIEnv
    m_connectivityManager = nullptr;
#endif

    m_initialized = false;
}

void AndroidNetwork::OnNetworkStateChanged(bool available, int type, bool metered)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_networkAvailable = available;
    m_isMetered = metered;

    switch (type)
    {
        case 1: m_networkType = NetworkType::WiFi; break;
        case 0: m_networkType = NetworkType::Mobile; break;
        case 9: m_networkType = NetworkType::Ethernet; break;
        default: m_networkType = available ? NetworkType::Unknown : NetworkType::None; break;
    }

    LOGI("Network state changed: available=%d, type=%s, metered=%d",
         available, GetNetworkTypeString().c_str(), metered);

    if (m_stateCallback)
    {
        m_stateCallback(available, m_networkType);
    }
}

void AndroidNetwork::UpdateNetworkState()
{
    // This would be called from Java via JNI to update state
    // For now, assume network is available
    m_networkAvailable = true;
}

std::string AndroidNetwork::ResolveHostname(const std::string& hostname)
{
    struct hostent* he = gethostbyname(hostname.c_str());
    if (!he)
        return "";

    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, he->h_addr_list[0], ip, INET_ADDRSTRLEN);
    return ip;
}

std::vector<std::string> AndroidNetwork::ResolveHostnameAll(const std::string& hostname)
{
    std::vector<std::string> result;

    struct hostent* he = gethostbyname(hostname.c_str());
    if (!he)
        return result;

    char ip[INET_ADDRSTRLEN];
    for (int i = 0; he->h_addr_list[i] != nullptr; i++)
    {
        inet_ntop(AF_INET, he->h_addr_list[i], ip, INET_ADDRSTRLEN);
        result.push_back(ip);
    }

    return result;
}

std::vector<ServerInfo> AndroidNetwork::QueryMasterServer(const std::string& masterUrl)
{
    std::vector<ServerInfo> servers;

    HttpClient http;
    http.SetTimeout(10000);

    HttpResponse response = http.Get(masterUrl);
    if (!response.success)
    {
        LOGE("Failed to query master server: %s", response.error.c_str());
        return servers;
    }

    // Parse server list (format depends on master server protocol)
    // This is a simplified implementation
    std::string data(response.body.begin(), response.body.end());

    // Each line: IP:PORT
    size_t pos = 0;
    while (pos < data.length())
    {
        size_t endPos = data.find('\n', pos);
        if (endPos == std::string::npos)
            endPos = data.length();

        std::string line = data.substr(pos, endPos - pos);
        pos = endPos + 1;

        // Remove \r if present
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        if (line.empty())
            continue;

        // Parse IP:PORT
        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos)
        {
            ServerInfo info;
            info.address = line.substr(0, colonPos);
            info.port = std::stoi(line.substr(colonPos + 1));
            info.players = 0;
            info.maxPlayers = 0;
            info.ping = -1;
            servers.push_back(info);
        }
    }

    LOGI("Got %zu servers from master server", servers.size());
    return servers;
}

bool AndroidNetwork::QueryServerInfo(const std::string& address, uint16_t port, ServerInfo& outInfo)
{
    Socket socket(Socket::Type::UDP);
    socket.SetTimeout(3000);

    // Send ASE query
    char query[] = { MTAProtocol::PACKET_QUERY };
    uint16_t queryPort = port + MTAProtocol::ASE_PORT_OFFSET;

    if (socket.SendTo(query, sizeof(query), address, queryPort) < 0)
        return false;

    // Receive response
    char buffer[2048];
    std::string fromHost;
    uint16_t fromPort;

    int received = socket.ReceiveFrom(buffer, sizeof(buffer) - 1, fromHost, fromPort);
    if (received <= 0)
        return false;

    // Parse ASE response (simplified)
    // Real implementation would parse the ASE protocol properly
    outInfo.address = address;
    outInfo.port = port;
    outInfo.name = "Unknown Server";
    outInfo.players = 0;
    outInfo.maxPlayers = 32;
    outInfo.ping = 50;  // Would measure actual ping

    return true;
}

int AndroidNetwork::PingServer(const std::string& address, uint16_t port)
{
    auto start = std::chrono::high_resolution_clock::now();

    Socket socket(Socket::Type::UDP);
    socket.SetTimeout(3000);

    char query[] = { MTAProtocol::PACKET_QUERY };
    uint16_t queryPort = port + MTAProtocol::ASE_PORT_OFFSET;

    if (socket.SendTo(query, sizeof(query), address, queryPort) < 0)
        return -1;

    char buffer[64];
    std::string fromHost;
    uint16_t fromPort;

    if (socket.ReceiveFrom(buffer, sizeof(buffer), fromHost, fromPort) <= 0)
        return -1;

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    return duration.count();
}

std::string AndroidNetwork::GetLocalIPAddress()
{
    // Create a UDP socket and connect to a public IP
    // This gives us our local IP without sending any data
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
        return "";

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("8.8.8.8");
    addr.sin_port = htons(53);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        close(sock);
        return "";
    }

    struct sockaddr_in localAddr;
    socklen_t len = sizeof(localAddr);
    if (getsockname(sock, (struct sockaddr*)&localAddr, &len) < 0)
    {
        close(sock);
        return "";
    }

    close(sock);

    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &localAddr.sin_addr, ip, INET_ADDRSTRLEN);
    return ip;
}

bool AndroidNetwork::IsLocalAddress(const std::string& address)
{
    // Check for localhost
    if (address == "127.0.0.1" || address == "localhost")
        return true;

    // Check for private IP ranges
    unsigned int a, b, c, d;
    if (sscanf(address.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) != 4)
        return false;

    // 10.x.x.x
    if (a == 10)
        return true;

    // 172.16.x.x - 172.31.x.x
    if (a == 172 && b >= 16 && b <= 31)
        return true;

    // 192.168.x.x
    if (a == 192 && b == 168)
        return true;

    return false;
}

std::string AndroidNetwork::GetMACAddress()
{
    // On modern Android, MAC address access is restricted
    // Would need to use NetworkInterface via JNI
    return "";
}

} // namespace MTA::Android::Platform
