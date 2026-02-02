#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <string>
#include <array>
#include <vector>
#include "../pool/proxy_pool.hpp"

namespace Mojo {
namespace Proxy {
namespace Server {

// Forward declare or use from Pool
using Pool::ProxyPool;
using Pool::Proxy;

class ProxyServer;

class Connection : public std::enable_shared_from_this<Connection> {
public:
    Connection(boost::asio::ip::tcp::socket socket, ProxyServer* server);
    ~Connection();

    void start();
    
private:
    void on_client_read(boost::system::error_code ec, std::size_t length);
    void on_resolve(boost::system::error_code ec, boost::asio::ip::tcp::resolver::results_type results);
    void on_upstream_connect(boost::system::error_code ec, const boost::asio::ip::tcp::endpoint& endpoint);
    void on_socks5_greeting_sent(boost::system::error_code ec, std::size_t length);
    void on_socks5_greeting_read(boost::system::error_code ec, std::size_t length);
    void on_socks5_request_sent(boost::system::error_code ec, std::size_t length);
    void on_socks5_response_read(boost::system::error_code ec, std::size_t length);
    void on_tunnel_write_upstream(boost::system::error_code ec, std::size_t length);
    void on_tunnel_read_upstream(boost::system::error_code ec, std::size_t length);
    void on_tunnel_write_client(boost::system::error_code ec, std::size_t length);
    void on_tunnel_read_client(boost::system::error_code ec, std::size_t length);
    void on_handshake_write_client(boost::system::error_code ec, std::size_t length);

    void do_read_client();
    void do_resolve();
    void do_connect_upstream(const boost::asio::ip::tcp::resolver::results_type& endpoints);
    void do_socks5_handshake();
    void start_tunnel();
    void close();

    struct Target {
        std::string host;
        int port = 0;
        bool is_connect = false;
    };
    bool parse_target(const std::string& data);

    boost::asio::ip::tcp::socket client_socket_;
    boost::asio::ip::tcp::socket upstream_socket_;
    boost::asio::ip::tcp::resolver resolver_;
    ProxyServer* server_;
    
    boost::asio::streambuf client_buffer_;
    boost::asio::streambuf upstream_buffer_;
    
    std::string initial_data_;
    Target target_;
    std::optional<Proxy> current_proxy_;
    
    static constexpr size_t kBufferSize = 8192;
    std::vector<char> tunnel_buffer_c2u_;
    std::vector<char> tunnel_buffer_u2c_;
};

}
}
}
