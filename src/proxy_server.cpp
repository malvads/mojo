#include <iostream>
#include <iostream>
#include <thread>
#include <vector>
#include <sstream>
#include <cstring>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    
    using socklen_t = int;
    #define close closesocket
    #define SHUT_RDWR SD_BOTH
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <arpa/inet.h>
    #include <fcntl.h>
#endif

#include "mojo/proxy_server.hpp"
#include "mojo/logger.hpp"
#include "mojo/url.hpp"

namespace Mojo {

namespace {
    constexpr int BUFFER_SIZE = 8192;
    constexpr int CONNECTION_TIMEOUT_SEC = 60;
    
    constexpr char SOCKS_VER_5 = 0x05;
    constexpr char SOCKS_AUTH_NONE = 0x00;
    constexpr char SOCKS_AUTH_USERPASS = 0x02;
    constexpr char SOCKS_CMD_CONNECT = 0x01;
    constexpr char SOCKS_ATYP_DOMAIN = 0x03;
    constexpr char SOCKS_ATYP_IPV4 = 0x01;
    constexpr char SOCKS_ATYP_IPV6 = 0x04;

    constexpr char SOCKS_VER_4 = 0x04;
    constexpr char SOCKS_CMD_CONNECT_V4 = 0x01;

    std::string base64_encode(const std::string& in) {
        std::string out;
        int val = 0, valb = -6;
        for (unsigned char c : in) {
            val = (val << 8) + c;
            valb += 8;
            while (valb >= 0) {
                out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(val >> valb) & 0x3F]);
                valb -= 6;
            }
        }
        if (valb > -6) out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[((val << 8) >> (valb + 8)) & 0x3F]);
        while (out.size() % 4) out.push_back('=');
        return out;
    }
}

ProxyServer::ProxyServer(ProxyPool& proxy_pool, const std::string& bind_ip, int bind_port) 
    : proxy_pool_(proxy_pool), bind_ip_(bind_ip), bind_port_(bind_port) {
    #ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
    #endif
}

ProxyServer::~ProxyServer() {
    stop();
    #ifdef _WIN32
        WSACleanup();
    #endif
}

void ProxyServer::start() {
    if (running_) return;
    
    server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_ < 0) {
        Logger::error("ProxyServer: Failed to create socket");
        return;
    }

    struct sockaddr_in serv_addr;
    std::memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(bind_ip_.c_str());
    serv_addr.sin_port = htons(bind_port_); // Use configured port (or 0 for random)

    if (bind(server_socket_, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        Logger::error("ProxyServer: Failed to bind to " + bind_ip_ + ":" + std::to_string(bind_port_));
        close(server_socket_);
        return;
    }

    socklen_t len = sizeof(serv_addr);
    if (getsockname(server_socket_, (struct sockaddr *)&serv_addr, &len) == -1) {
        Logger::error("ProxyServer: getsockname failed");
        close(server_socket_);
        return;
    }
    port_ = ntohs(serv_addr.sin_port);

    listen(server_socket_, 50);
    running_ = true;
    Logger::info("ProxyServer: Listening on " + bind_ip_ + ":" + std::to_string(port_));

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

void ProxyServer::handle_client(int client_socket) {
    std::string initial_buffer;
    ssize_t initial_len = 0;
    
    auto target_opt = parse_initial_request(client_socket, initial_buffer, initial_len);
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

    bool handshake_success = false;

    if (p_info.scheme.find("socks5") != std::string::npos) {
        handshake_success = perform_socks5_handshake(upstream_sock, target.host, target.port, p_info.user, p_info.pass);
        if (!handshake_success) Logger::error("ProxyServer: SOCKS5 handshake failed: " + p_info.host);
    } 
    else if (p_info.scheme.find("socks4") != std::string::npos) {
        handshake_success = perform_socks4_handshake(upstream_sock, target.host, target.port, p_info.user);
        if (!handshake_success) Logger::error("ProxyServer: SOCKS4 handshake failed: " + p_info.host);
    } 
    else {
        if (!p_info.user.empty()) {
            inject_http_auth(initial_buffer, p_info.user, p_info.pass);
        }
        handshake_success = true;
    }

    if (handshake_success) {
        bool is_socks = p_info.scheme.find("socks") != std::string::npos;

        if (target.is_connect && is_socks) {
            const std::string ok = "HTTP/1.1 200 Connection Established\r\n\r\n";
            send(client_socket, ok.c_str(), ok.size(), 0);
        } 
        else {
            // HTTP request via SOCKS or any request via HTTP Proxy
            // If HTTP Proxy, we send the (potentially modified) initial buffer
            // If SOCKS, we send the raw HTTP buffer through the tunnel
            if (!target.is_connect || !is_socks) {
                 send(upstream_sock, initial_buffer.data(), initial_buffer.size(), 0);
            }
        }
        
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
        std::string auth = p_url.substr(0, at_pos);
        size_t colon = auth.find(':');
        if (colon != std::string::npos) {
            info.user = auth.substr(0, colon);
            info.pass = auth.substr(colon + 1);
        } else {
            info.user = auth;
        }
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

bool ProxyServer::perform_socks5_handshake(int upstream_sock, const std::string& target_host, int target_port, const std::string& user, const std::string& pass) {
    bool auth_required = !user.empty();
    std::vector<char> greeting = { SOCKS_VER_5, 0x01, auth_required ? SOCKS_AUTH_USERPASS : SOCKS_AUTH_NONE };
    
    if (send(upstream_sock, greeting.data(), greeting.size(), 0) != (ssize_t)greeting.size()) return false;

    char resp[2];
    if (recv(upstream_sock, resp, 2, 0) != 2 || resp[0] != SOCKS_VER_5) return false;

    if (resp[1] == SOCKS_AUTH_USERPASS) {
        if (!authenticate_socks5(upstream_sock, user, pass)) return false;
    } else if (resp[1] != SOCKS_AUTH_NONE) {
        return false; // Unsupported auth method
    }

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

    char header[4];
    if (recv(upstream_sock, header, 4, 0) != 4) return false;
    if (header[1] != 0x00) return false; 

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
        char dump[256];
        recv(upstream_sock, dump, skip, 0); 
    }
    
    return true;
}

bool ProxyServer::authenticate_socks5(int upstream_sock, const std::string& user, const std::string& pass) {
    std::vector<char> auth_req;
    auth_req.push_back(0x01); // Subnegotiation version
    auth_req.push_back(static_cast<char>(user.size()));
    auth_req.insert(auth_req.end(), user.begin(), user.end());
    auth_req.push_back(static_cast<char>(pass.size()));
    auth_req.insert(auth_req.end(), pass.begin(), pass.end());

    if (send(upstream_sock, auth_req.data(), auth_req.size(), 0) != (ssize_t)auth_req.size()) return false;

    char resp[2];
    if (recv(upstream_sock, resp, 2, 0) != 2) return false;
    return resp[1] == 0x00; // Success
}

bool ProxyServer::perform_socks4_handshake(int upstream_sock, const std::string& target_host, int target_port, const std::string& user) {
    struct hostent *he = gethostbyname(target_host.c_str());
    if (!he || he->h_addrtype != AF_INET) return false;

    std::vector<char> req;
    req.push_back(SOCKS_VER_4);
    req.push_back(SOCKS_CMD_CONNECT_V4);
    
    req.push_back(static_cast<char>((target_port >> 8) & 0xFF));
    req.push_back(static_cast<char>(target_port & 0xFF));
    
    req.push_back(he->h_addr_list[0][0]);
    req.push_back(he->h_addr_list[0][1]);
    req.push_back(he->h_addr_list[0][2]);
    req.push_back(he->h_addr_list[0][3]);
    
    // UserID
    if (!user.empty()) {
        req.insert(req.end(), user.begin(), user.end());
    }
    req.push_back(0x00);
    
    if (send(upstream_sock, req.data(), req.size(), 0) != (ssize_t)req.size()) return false;

    char resp[8];
    if (recv(upstream_sock, resp, 8, 0) != 8) return false;
    
    return resp[1] == 0x5A;
}

void ProxyServer::inject_http_auth(std::string& buffer, const std::string& user, const std::string& pass) {
    std::string auth_str = user + ":" + pass;
    std::string auth_header = "Proxy-Authorization: Basic " + base64_encode(auth_str) + "\r\n";
    
    size_t first_crlf = buffer.find("\r\n");
    if (first_crlf != std::string::npos) {
        buffer.insert(first_crlf + 2, auth_header);
    }
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
        if (activity <= 0) break;
        
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
