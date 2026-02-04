#include "connection.hpp"
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>
#include "../../binary/reader.hpp"
#include "../../binary/writer.hpp"
#include "../../core/logger/logger.hpp"
#include "../../network/proxy/socks_handshake.hpp"
#include "../../utils/url/url.hpp"
#include "proxy_server.hpp"

namespace Mojo {
namespace Proxy {
namespace Server {

using namespace Mojo::Core;
using Pool::Proxy;

Connection::Connection(boost::asio::ip::tcp::socket socket, ProxyServer* server)
    : client_socket_(std::move(socket)),
      upstream_socket_(server->io_context()),
      resolver_(server->io_context()),
      server_(server),
      tunnel_buffer_c2u_(kBufferSize),
      tunnel_buffer_u2c_(kBufferSize) {
}

Connection::~Connection() {
    close();
}

void Connection::start() {
    boost::asio::co_spawn(
        server_->io_context(),
        [self = shared_from_this()]() { return self->start_impl(); },
        boost::asio::detached);
}

void Connection::close() {
    boost::system::error_code ec;
    if (client_socket_.is_open())
        client_socket_.close(ec);
    if (upstream_socket_.is_open())
        upstream_socket_.close(ec);
}

boost::asio::awaitable<void> Connection::start_impl() {
    try {
        std::size_t n = co_await client_socket_.async_read_some(client_buffer_.prepare(8192),
                                                                boost::asio::use_awaitable);
        client_buffer_.commit(n);

        auto        bufs = client_buffer_.data();
        std::string data(boost::asio::buffers_begin(bufs),
                         boost::asio::buffers_begin(bufs) + client_buffer_.size());
        initial_data_ = data;

        if (!parse_target_request(data)) {
            close();
            co_return;
        }

        co_await do_resolve();

    } catch (const std::exception& e) {
        close();
    }
}

boost::asio::awaitable<void> Connection::do_resolve() {
    current_proxy_ = server_->proxy_pool().get_proxy();
    if (!current_proxy_) {
        close();
        co_return;
    }

    auto        url_parsed = Mojo::Utils::Url::parse(current_proxy_->url);
    std::string p_host     = url_parsed.host;
    std::string p_port     = url_parsed.port.empty() ? "80" : url_parsed.port;

    try {
        auto results = co_await resolver_.async_resolve(p_host, p_port, boost::asio::use_awaitable);
        co_await                do_connect_upstream(results);
    } catch (...) {
        server_->proxy_pool().report(*current_proxy_, false);
        throw;
    }
}

boost::asio::awaitable<void>
Connection::do_connect_upstream(const boost::asio::ip::tcp::resolver::results_type& endpoints) {
    try {
        co_await boost::asio::async_connect(
            upstream_socket_, endpoints, boost::asio::use_awaitable);
    } catch (...) {
        server_->proxy_pool().report(*current_proxy_, false);
        throw;
    }

    if (current_proxy_->url.find("socks5") != std::string::npos) {
        co_await do_socks5_handshake();
    }
    else {
        co_await boost::asio::async_write(
            upstream_socket_, boost::asio::buffer(initial_data_), boost::asio::use_awaitable);
        co_await start_tunnel();
    }
}

boost::asio::awaitable<void> Connection::do_socks5_handshake() {
    co_await Mojo::Network::Proxy::SocksHandshake::perform_socks5(
        upstream_socket_, target_.host, std::to_string(target_.port));

    if (target_.is_connect) {
        std::string msg = "HTTP/1.1 200 Connection Established\r\n\r\n";
        co_await    boost::asio::async_write(
            client_socket_, boost::asio::buffer(msg), boost::asio::use_awaitable);
    }
    else {
        co_await boost::asio::async_write(
            upstream_socket_, boost::asio::buffer(initial_data_), boost::asio::use_awaitable);
    }
    co_await start_tunnel();
}

boost::asio::awaitable<void> Connection::start_tunnel() {
    server_->proxy_pool().report(*current_proxy_, true);

    boost::asio::co_spawn(
        server_->io_context(),
        [self = shared_from_this()]() {
            return self->transfer(
                self->client_socket_, self->upstream_socket_, self->tunnel_buffer_c2u_);
        },
        boost::asio::detached);

    boost::asio::co_spawn(
        server_->io_context(),
        [self = shared_from_this()]() {
            return self->transfer(
                self->upstream_socket_, self->client_socket_, self->tunnel_buffer_u2c_);
        },
        boost::asio::detached);

    co_return;
}

boost::asio::awaitable<void> Connection::transfer(boost::asio::ip::tcp::socket& from,
                                                  boost::asio::ip::tcp::socket& to,
                                                  std::vector<char>&            buffer) {
    try {
        while (true) {
            std::size_t n = co_await from.async_read_some(boost::asio::buffer(buffer),
                                                          boost::asio::use_awaitable);
            co_await                 boost::asio::async_write(
                to, boost::asio::buffer(buffer, n), boost::asio::use_awaitable);
        }
    } catch (...) {
        close();
    }
}

bool Connection::parse_target_request(const std::string& data) {
    if (data.rfind("CONNECT", 0) == 0) {
        target_.is_connect = true;
        auto sp1           = data.find(' ');
        auto sp2           = data.find(' ', sp1 + 1);
        if (sp1 == std::string::npos || sp2 == std::string::npos)
            return false;

        std::string host_port = data.substr(sp1 + 1, sp2 - sp1 - 1);
        auto        colon     = host_port.find(':');
        if (colon != std::string::npos) {
            target_.host = host_port.substr(0, colon);
            try {
                target_.port = std::stoi(host_port.substr(colon + 1));
            } catch (...) {
                return false;
            }
        }
        else {
            target_.host = host_port;
            target_.port = 443;
        }
        return true;
    }
    else {
        target_.is_connect = false;
        auto pos           = data.find("\nHost: ");
        if (pos == std::string::npos)
            pos = data.find("\r\nHost: ");
        if (pos == std::string::npos)
            return false;

        pos += (data[pos] == '\r' ? 8 : 7);
        auto end = data.find('\r', pos);
        if (end == std::string::npos)
            end = data.find('\n', pos);

        std::string host_line = data.substr(pos, end - pos);
        auto        colon     = host_line.find(':');
        if (colon != std::string::npos) {
            target_.host = host_line.substr(0, colon);
            try {
                target_.port = std::stoi(host_line.substr(colon + 1));
            } catch (...) {
                return false;
            }
        }
        else {
            target_.host = host_line;
            target_.port = 80;
        }
        return true;
    }
}

}  // namespace Server
}  // namespace Proxy
}  // namespace Mojo
