#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include "mojo/proxy_pool.hpp"

namespace Mojo {

class ProxyServer {
public:
    explicit ProxyServer(ProxyPool& proxy_pool);
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
    int server_socket_ = -1;
    int port_ = 0;

    void accept_loop();
    void handle_client(int client_socket);
    
    // Helpers
    std::optional<TargetInfo> parse_initial_request(int client_socket, std::string& out_buffer, ssize_t& out_len);
    int connect_to_upstream(const std::string& host, int port);
    bool perform_socks5_handshake(int upstream_sock, const std::string& target_host, int target_port);
    bool perform_socks4_handshake(int upstream_sock, const std::string& target_host, int target_port);
    void tunnel_traffic(int client_sock, int upstream_sock);
    
    struct ParsedProxy {
        std::string host;
        int port;
        std::string scheme;
        std::string auth;
    };
    ParsedProxy parse_proxy_url(const std::string& url);
};

}
