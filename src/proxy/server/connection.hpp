#pragma once

#include <array>
#include <boost/asio.hpp>
#include <memory>
#include <string>
#include <vector>
#include "../pool/proxy_pool.hpp"

namespace Mojo {
namespace Proxy {
namespace Server {

// Forward declare or use from Pool
using Pool::Proxy;
using Pool::ProxyPool;

class ProxyServer;

class Connection : public std::enable_shared_from_this<Connection> {
public:
    Connection(boost::asio::ip::tcp::socket socket, ProxyServer* server);
    ~Connection();

    void start();

private:
    boost::asio::awaitable<void> start_impl();
    boost::asio::awaitable<void> do_resolve();
    boost::asio::awaitable<void>
    do_connect_upstream(const boost::asio::ip::tcp::resolver::results_type& endpoints);
    boost::asio::awaitable<void> do_socks5_handshake();
    boost::asio::awaitable<void> start_tunnel();
    boost::asio::awaitable<void> transfer(boost::asio::ip::tcp::socket& from,
                                          boost::asio::ip::tcp::socket& to,
                                          std::vector<char>&            buffer);
    void                         close();

    struct Target {
        std::string host;
        int         port       = 0;
        bool        is_connect = false;
    };
    bool parse_target_request(const std::string& data);

    boost::asio::ip::tcp::socket   client_socket_;
    boost::asio::ip::tcp::socket   upstream_socket_;
    boost::asio::ip::tcp::resolver resolver_;
    ProxyServer*                   server_;

    boost::asio::streambuf client_buffer_;
    boost::asio::streambuf upstream_buffer_;

    std::string          initial_data_;
    Target               target_;
    std::optional<Proxy> current_proxy_;

    static constexpr size_t kBufferSize = 8192;
    std::vector<char>       tunnel_buffer_c2u_;
    std::vector<char>       tunnel_buffer_u2c_;
};

}  // namespace Server
}  // namespace Proxy
}  // namespace Mojo
