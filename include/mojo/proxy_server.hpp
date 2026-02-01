#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include "mojo/proxy_pool.hpp"

namespace Mojo {

#include <cstdint>

#ifdef _WIN32
    using ssize_t = long long;
    using SocketHandle = unsigned long long; // UINT_PTR equivalent
#else
    using SocketHandle = int;
#endif

class ProxyServer {
public:
    explicit ProxyServer(ProxyPool& proxy_pool, const std::string& bind_ip = "127.0.0.1", int bind_port = 0);
    ~ProxyServer();

    void start();
    void stop();
    int get_port() const;

private:
    struct TargetInfo {
        std::string host;
        int port = 80;
        bool is_connect = false;
    };

    ProxyPool& proxy_pool_;
    std::atomic<bool> running_{false};
    std::thread server_thread_;
    SocketHandle server_socket_ = -1;
    int port_ = 0;
    std::string bind_ip_ = "127.0.0.1";
    int bind_port_ = 0;

    void accept_loop();
    void handle_client(SocketHandle client_socket);
    
    std::optional<TargetInfo> parse_initial_request(SocketHandle client_socket, std::string& out_buffer, ssize_t& out_len);
    SocketHandle connect_to_upstream(const std::string& host, int port);
    
    bool perform_socks5_handshake(SocketHandle upstream_sock, const std::string& target_host, int target_port, const std::string& user, const std::string& pass);
    bool perform_socks4_handshake(SocketHandle upstream_sock, const std::string& target_host, int target_port, const std::string& user);
    bool authenticate_socks5(SocketHandle upstream_sock, const std::string& user, const std::string& pass);
    void inject_http_auth(std::string& buffer, const std::string& user, const std::string& pass);
    void tunnel_traffic(SocketHandle client_sock, SocketHandle upstream_sock);
    
    struct ParsedProxy {
        std::string host;
        int port;
        std::string scheme;
        std::string user;
        std::string pass;
    };
    ParsedProxy parse_proxy_url(const std::string& url);
};

}
