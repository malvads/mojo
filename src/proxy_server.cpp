#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <cstring>
#include <thread>
#include <vector>

#include "mojo/proxy_server.hpp"
#include "mojo/logger.hpp"
#include "mojo/url.hpp"

namespace Mojo {

ProxyServer::ProxyServer(ProxyPool& proxy_pool) : proxy_pool_(proxy_pool) {}

ProxyServer::~ProxyServer() {
    stop();
}

void ProxyServer::start() {
    if (running_) return;
    
    // Create socket
    server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_ < 0) {
        Logger::error("ProxyServer: Failed to create socket");
        return;
    }

    // Bind to random port
    struct sockaddr_in serv_addr;
    std::memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serv_addr.sin_port = 0; // Random port

    if (bind(server_socket_, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        Logger::error("ProxyServer: Failed to bind");
        close(server_socket_);
        return;
    }

    // Get assigned port
    socklen_t len = sizeof(serv_addr);
    if (getsockname(server_socket_, (struct sockaddr *)&serv_addr, &len) == -1) {
        Logger::error("ProxyServer: getsockname failed");
        close(server_socket_);
        return;
    }
    port_ = ntohs(serv_addr.sin_port);

    listen(server_socket_, 50);
    running_ = true;
    Logger::info("ProxyServer: Listening on 127.0.0.1:" + std::to_string(port_));

    server_thread_ = std::thread(&ProxyServer::accept_loop, this);
}

void ProxyServer::stop() {
    if (!running_) return;
    running_ = false;
    if (server_socket_ != -1) {
        close(server_socket_);
        server_socket_ = -1;
    }
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
}

int ProxyServer::get_port() const {
    return port_;
}

void ProxyServer::accept_loop() {
    while (running_) {
        struct sockaddr_in cli_addr;
        socklen_t clilen = sizeof(cli_addr);
        int client_sock = accept(server_socket_, (struct sockaddr *) &cli_addr, &clilen);
        
        if (client_sock < 0) {
            if (running_) Logger::warn("ProxyServer: Accept failed");
            continue;
        }

        std::thread(&ProxyServer::handle_client, this, client_sock).detach();
    }
}

namespace {
    constexpr int BUFFER_SIZE = 8192;
    constexpr int CONNECTION_TIMEOUT_SEC = 60;
    
    // SOCKS5 Constants
    constexpr char SOCKS_VER_5 = 0x05;
    constexpr char SOCKS_AUTH_NONE = 0x00;
    constexpr char SOCKS_CMD_CONNECT = 0x01;
    constexpr char SOCKS_ATYP_DOMAIN = 0x03;
    constexpr char SOCKS_ATYP_IPV4 = 0x01;
    constexpr char SOCKS_ATYP_IPV6 = 0x04;

    // SOCKS4 Constants
    constexpr char SOCKS_VER_4 = 0x04;
    constexpr char SOCKS_CMD_CONNECT_V4 = 0x01;
}

void ProxyServer::handle_client(int client_socket) {
    std::string initial_buffer_str;
    ssize_t initial_n = 0;
    
    auto target_opt = parse_initial_request(client_socket, initial_buffer_str, initial_n);
    if (!target_opt) {
        close(client_socket);
        return;
    }
    TargetInfo target = *target_opt;

    auto proxy_opt = proxy_pool_.get_proxy();
    if (!proxy_opt) {
        Logger::error("ProxyServer: Pool exhausted");
        close(client_socket);
        return;
    }

    ParsedProxy p_info = parse_proxy_url(proxy_opt->url);

    int upstream_sock = connect_to_upstream(p_info.host, p_info.port);
    if (upstream_sock < 0) {
        close(client_socket);
        proxy_pool_.report(*proxy_opt, false);
        return;
    }

    bool tunnel_ready = false;
    bool handshake_success = false;

    if (p_info.scheme.find("socks5") != std::string::npos) {
        handshake_success = perform_socks5_handshake(upstream_sock, target.host, target.port);
        if (!handshake_success) Logger::error("ProxyServer: SOCKS5 handshake failed: " + p_info.host);
    } 
    else if (p_info.scheme.find("socks4") != std::string::npos) {
        handshake_success = perform_socks4_handshake(upstream_sock, target.host, target.port);
        if (!handshake_success) Logger::error("ProxyServer: SOCKS4 handshake failed: " + p_info.host);
    } 
    else {
        // HTTP/HTTPS Proxy: Blind forwarding
        // Generally standard HTTP proxies accept CONNECT/GET as-is.
        handshake_success = true;
    }

    if (handshake_success) {
        if (target.is_connect && (p_info.scheme.find("socks") != std::string::npos)) {
            // HTTPS via SOCKS: We must simulate the HTTP 200 OK because the SOCKS server just gives raw TCP.
            const std::string ok = "HTTP/1.1 200 Connection Established\r\n\r\n";
            send(client_socket, ok.c_str(), ok.size(), 0);
        } else {
            // HTTP via SOCKS / HTTPS via HTTP Proxy
            if (!target.is_connect || (p_info.scheme.find("socks") == std::string::npos)) {
                 send(upstream_sock, initial_buffer_str.data(), initial_n, 0);
            }
        }
        tunnel_ready = true;
    }

    if (tunnel_ready) {
        Logger::info("Tunnel: Chrome -> " + p_info.host + " -> " + target.host);
        tunnel_traffic(client_socket, upstream_sock);
        proxy_pool_.report(*proxy_opt, true);
    } else {
        proxy_pool_.report(*proxy_opt, false);
    }

    close(client_socket);
    close(upstream_sock);
}

std::optional<ProxyServer::TargetInfo> ProxyServer::parse_initial_request(int client_socket, std::string& out_buffer, ssize_t& out_len) {
    char buffer[BUFFER_SIZE];
    out_len = recv(client_socket, buffer, sizeof(buffer), 0);
    if (out_len <= 0) return std::nullopt;
    
    out_buffer.assign(buffer, out_len);
    TargetInfo info;
    
    // Parse CONNECT (HTTPS)
    if (out_buffer.find("CONNECT") == 0) {
        info.is_connect = true;
        size_t space1 = out_buffer.find(' ');
        size_t space2 = out_buffer.find(' ', space1 + 1);
        if (space1 != std::string::npos && space2 != std::string::npos) {
            std::string url = out_buffer.substr(space1 + 1, space2 - space1 - 1);
            size_t colon = url.find(':');
            if (colon != std::string::npos) {
                info.host = url.substr(0, colon);
                info.port = std::stoi(url.substr(colon + 1));
            } else {
                info.host = url;
            }
        }
    } else {
        // Parse GET/POST (HTTP) via Host header
        info.is_connect = false;
        size_t host_pos = out_buffer.find("Host: ");
        if (host_pos != std::string::npos) {
            size_t end_pos = out_buffer.find("\r\n", host_pos);
            if (end_pos != std::string::npos) {
                std::string host_line = out_buffer.substr(host_pos + 6, end_pos - host_pos - 6);
                size_t colon = host_line.find(':');
                if (colon != std::string::npos) {
                    info.host = host_line.substr(0, colon);
                    info.port = std::stoi(host_line.substr(colon + 1));
                } else {
                    info.host = host_line;
                }
            }
        }
    }
    
    if (info.host.empty()) return std::nullopt;
    return info;
}

ProxyServer::ParsedProxy ProxyServer::parse_proxy_url(const std::string& url_str) {
    ParsedProxy info;
    info.scheme = "http";
    info.port = 80;
    
    std::string p_url = url_str;
    size_t scheme_end = p_url.find("://");
    if (scheme_end != std::string::npos) {
        info.scheme = p_url.substr(0, scheme_end);
        p_url = p_url.substr(scheme_end + 3);
    }
    
    size_t at_pos = p_url.find('@');
    if (at_pos != std::string::npos) {
        info.auth = p_url.substr(0, at_pos);
        p_url = p_url.substr(at_pos + 1);
    }
    
    size_t colon = p_url.find(':');
    if (colon != std::string::npos) {
        info.host = p_url.substr(0, colon);
        info.port = std::stoi(p_url.substr(colon + 1));
    } else {
        info.host = p_url;
    }
    return info;
}

int ProxyServer::connect_to_upstream(const std::string& host, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    struct hostent *server = gethostbyname(host.c_str());
    if (server == NULL) {
        Logger::error("ProxyServer: DNS resolution failed for " + host);
        close(sock);
        return -1;
    }

    struct sockaddr_in serv_addr;
    std::memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    std::memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    serv_addr.sin_port = htons(port);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sock);
        return -1;
    }
    return sock;
}

bool ProxyServer::perform_socks5_handshake(int upstream_sock, const std::string& target_host, int target_port) {
    // 1. Auth Negotiation (Support No Auth)
    char greeting[] = { SOCKS_VER_5, 0x01, SOCKS_AUTH_NONE };
    if (send(upstream_sock, greeting, sizeof(greeting), 0) != sizeof(greeting)) return false;

    char resp[2];
    if (recv(upstream_sock, resp, 2, 0) != 2 || resp[0] != SOCKS_VER_5 || resp[1] != SOCKS_AUTH_NONE) return false;

    // 2. Connection Request
    std::vector<char> req;
    req.push_back(SOCKS_VER_5); 
    req.push_back(SOCKS_CMD_CONNECT); 
    req.push_back(0x00); 
    req.push_back(SOCKS_ATYP_DOMAIN);
    
    req.push_back(static_cast<char>(target_host.size()));
    req.insert(req.end(), target_host.begin(), target_host.end());
    req.push_back(static_cast<char>((target_port >> 8) & 0xFF));
    req.push_back(static_cast<char>(target_port & 0xFF));

    if (send(upstream_sock, req.data(), req.size(), 0) != (ssize_t)req.size()) return false;

    // 3. Read Response Header (VER, REP, RSV, ATYP)
    char header[4];
    if (recv(upstream_sock, header, 4, 0) != 4) return false;
    if (header[1] != 0x00) return false; // Non-zero REP indicates failure

    // 4. Drain Address Field
    int skip = 0;
    switch (header[3]) {
        case SOCKS_ATYP_IPV4: skip = 4 + 2; break;
        case SOCKS_ATYP_IPV6: skip = 16 + 2; break;
        case SOCKS_ATYP_DOMAIN: {
            char len;
            if (recv(upstream_sock, &len, 1, 0) != 1) return false;
            skip = len + 2; 
            break;
        }
        default: return false;
    }
    
    if (skip > 0) {
        char dump[256]; // Max domain len is 255
        recv(upstream_sock, dump, skip, 0); 
    }
    
    return true;
}

bool ProxyServer::perform_socks4_handshake(int upstream_sock, const std::string& target_host, int target_port) {
    // SOCKS4 Connect Request:
    // VER(1) CMD(1) DSTPORT(2) DSTIP(4) USERID(var) NULL(1)
    
    // Resolve target host to IPv4 (SOCKS4 doesn't support domain resolution typically, SOCKS4a does)
    // We will attempt to resolve here.
    struct hostent *he = gethostbyname(target_host.c_str());
    if (!he || he->h_addrtype != AF_INET) {
        // Fallback or fail. SOCKS4a could be supported but let's stick to SOCKS4 (IPv4)
        return false;
    }

    std::vector<char> req;
    req.push_back(SOCKS_VER_4);
    req.push_back(SOCKS_CMD_CONNECT_V4);
    
    req.push_back(static_cast<char>((target_port >> 8) & 0xFF));
    req.push_back(static_cast<char>(target_port & 0xFF));
    
    // IP Address (4 bytes)
    req.push_back(he->h_addr_list[0][0]);
    req.push_back(he->h_addr_list[0][1]);
    req.push_back(he->h_addr_list[0][2]);
    req.push_back(he->h_addr_list[0][3]);
    
    // UserID (Empty + NULL terminator)
    req.push_back(0x00);
    
    if (send(upstream_sock, req.data(), req.size(), 0) != (ssize_t)req.size()) return false;

    // Response: VN(1) CD(1) DSTPORT(2) DSTIP(4)
    // VN=0, CD=90(0x5A) means granted.
    char resp[8];
    if (recv(upstream_sock, resp, 8, 0) != 8) return false;
    
    if (resp[1] != 0x5A) return false; // 90 = Request granted
    
    return true;
}

void ProxyServer::tunnel_traffic(int client_sock, int upstream_sock) {
    fd_set fds;
    int max_fd = std::max(client_sock, upstream_sock);
    char buffer[BUFFER_SIZE];
    
    while (true) {
        FD_ZERO(&fds);
        FD_SET(client_sock, &fds);
        FD_SET(upstream_sock, &fds);
        
        struct timeval tv; 
        tv.tv_sec = CONNECTION_TIMEOUT_SEC;
        tv.tv_usec = 0;
        
        int activity = select(max_fd + 1, &fds, NULL, NULL, &tv);
        if (activity <= 0) break; // Timeout or error
        
        if (FD_ISSET(client_sock, &fds)) {
            ssize_t n = recv(client_sock, buffer, sizeof(buffer), 0);
            if (n <= 0) break;
            send(upstream_sock, buffer, n, 0);
        }
        
        if (FD_ISSET(upstream_sock, &fds)) {
            ssize_t n = recv(upstream_sock, buffer, sizeof(buffer), 0);
            if (n <= 0) break;
            send(client_sock, buffer, n, 0);
        }
    }
}

}
