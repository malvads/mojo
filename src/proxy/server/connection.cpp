#include "connection.hpp"
#include "proxy_server.hpp"
#include "../../core/logger/logger.hpp"

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
    do_read_client();
}

void Connection::close() {
    boost::system::error_code ec;
    client_socket_.close(ec);
    upstream_socket_.close(ec);
}


void Connection::do_read_client() {
    client_socket_.async_read_some(client_buffer_.prepare(8192),
        std::bind(&Connection::on_client_read, shared_from_this(),
                  std::placeholders::_1, std::placeholders::_2));
}

void Connection::on_client_read(boost::system::error_code ec, std::size_t length) {
    if (ec) {
        close();
        return;
    }
    
    client_buffer_.commit(length);
    
    auto bufs = client_buffer_.data();
    std::string data(boost::asio::buffers_begin(bufs), boost::asio::buffers_begin(bufs) + client_buffer_.size());
    
    initial_data_ = data;
    
    if (parse_target(data)) {
        do_resolve();
    } else {
        close();
    }
}

bool Connection::parse_target(const std::string& data) {
    if (data.rfind("CONNECT", 0) == 0) {
        target_.is_connect = true;
        auto sp1 = data.find(' ');
        auto sp2 = data.find(' ', sp1 + 1);
        if (sp1 == std::string::npos || sp2 == std::string::npos) return false;
        
        std::string host_port = data.substr(sp1 + 1, sp2 - sp1 - 1);
        auto colon = host_port.find(':');
        if (colon != std::string::npos) {
            target_.host = host_port.substr(0, colon);
            try {
                target_.port = std::stoi(host_port.substr(colon + 1));
            } catch (...) { return false; }
        } else {
            target_.host = host_port;
            target_.port = 443;
        }
        return true;
    } else {
        target_.is_connect = false;
        auto pos = data.find("\nHost: ");
        if (pos == std::string::npos) pos = data.find("\r\nHost: ");
        if (pos == std::string::npos) return false;
        
        pos += (data[pos] == '\r' ? 8 : 7);
        auto end = data.find('\r', pos);
        if (end == std::string::npos) end = data.find('\n', pos);
        
        std::string host_line = data.substr(pos, end - pos);
        auto colon = host_line.find(':');
        if (colon != std::string::npos) {
            target_.host = host_line.substr(0, colon);
            try {
                target_.port = std::stoi(host_line.substr(colon + 1));
            } catch (...) { return false; }
        } else {
            target_.host = host_line;
            target_.port = 80;
        }
        return true;
    }
}


void Connection::do_resolve() {
    current_proxy_ = server_->proxy_pool().get_proxy();
    if (!current_proxy_) {
        close();
        return;
    }

    std::string p_host;
    std::string p_port = "80";
    
    std::string url = current_proxy_->url;
    auto scheme_end = url.find("://");
    if (scheme_end != std::string::npos) url = url.substr(scheme_end + 3);
    auto at = url.find('@');
    if (at != std::string::npos) url = url.substr(at + 1);
    auto colon = url.find(':');
    if (colon != std::string::npos) {
        p_host = url.substr(0, colon);
        p_port = url.substr(colon + 1);
    } else {
        p_host = url;
    }
    
    resolver_.async_resolve(p_host, p_port,
        std::bind(&Connection::on_resolve, shared_from_this(),
                  std::placeholders::_1, std::placeholders::_2));
}

void Connection::on_resolve(boost::system::error_code ec, boost::asio::ip::tcp::resolver::results_type results) {
    if (!ec) {
        do_connect_upstream(results);
    } else {
        server_->proxy_pool().report(*current_proxy_, false);
        close();
    }
}


void Connection::do_connect_upstream(const boost::asio::ip::tcp::resolver::results_type& endpoints) {
    boost::asio::async_connect(upstream_socket_, endpoints,
        std::bind(&Connection::on_upstream_connect, shared_from_this(),
                  std::placeholders::_1, std::placeholders::_2));
}

void Connection::on_upstream_connect(boost::system::error_code ec, const boost::asio::ip::tcp::endpoint&) {
    if (ec) {
        server_->proxy_pool().report(*current_proxy_, false);
        close();
        return;
    }

    if (current_proxy_->url.find("socks5") != std::string::npos) {
        do_socks5_handshake();
    } else {
        

        boost::asio::async_write(upstream_socket_, boost::asio::buffer(initial_data_),
             std::bind(&Connection::on_tunnel_write_upstream, shared_from_this(),
                       std::placeholders::_1, std::placeholders::_2));
    }
}


void Connection::do_socks5_handshake() {
    auto buf = std::make_shared<std::vector<char>>(std::initializer_list<char>{0x05, 0x01, 0x00});
    
    boost::asio::async_write(upstream_socket_, boost::asio::buffer(*buf),
        std::bind(&Connection::on_socks5_greeting_sent, shared_from_this(),
                  std::placeholders::_1, std::placeholders::_2));
}

void Connection::on_socks5_greeting_sent(boost::system::error_code ec, std::size_t) {
    if (ec) { close(); return; }
    
    upstream_buffer_.consume(upstream_buffer_.size()); // Clear
    boost::asio::async_read(upstream_socket_, upstream_buffer_.prepare(2),
         std::bind(&Connection::on_socks5_greeting_read, shared_from_this(),
                   std::placeholders::_1, std::placeholders::_2));
}

void Connection::on_socks5_greeting_read(boost::system::error_code ec, std::size_t length) {
    if (ec) { close(); return; }
    upstream_buffer_.commit(length);
    
    
    upstream_buffer_.consume(length); 

    auto req = std::make_shared<std::vector<char>>();
    req->push_back(0x05);
    req->push_back(0x01);
    req->push_back(0x00);
    req->push_back(0x03);
    req->push_back((char)target_.host.size());
    req->insert(req->end(), target_.host.begin(), target_.host.end());
    req->push_back((target_.port >> 8) & 0xFF);
    req->push_back(target_.port & 0xFF);
    
    boost::asio::async_write(upstream_socket_, boost::asio::buffer(*req),
        std::bind(&Connection::on_socks5_request_sent, shared_from_this(),
                  std::placeholders::_1, std::placeholders::_2));
}

void Connection::on_socks5_request_sent(boost::system::error_code ec, std::size_t) {
    if (ec) { close(); return; }
    
    boost::asio::async_read(upstream_socket_, upstream_buffer_.prepare(4),
         std::bind(&Connection::on_socks5_response_read, shared_from_this(),
                   std::placeholders::_1, std::placeholders::_2));
}

void Connection::on_socks5_response_read(boost::system::error_code ec, std::size_t length) {
    if (ec) { close(); return; }
    upstream_buffer_.commit(length);
    upstream_buffer_.consume(length); 
    
    if (target_.is_connect) {
        auto msg = std::make_shared<std::string>("HTTP/1.1 200 Connection Established\r\n\r\n");
        boost::asio::async_write(client_socket_, boost::asio::buffer(*msg),
             std::bind(&Connection::on_handshake_write_client, shared_from_this(),
                       std::placeholders::_1, std::placeholders::_2));
    } else {
        boost::asio::async_write(upstream_socket_, boost::asio::buffer(initial_data_),
             std::bind(&Connection::on_tunnel_write_upstream, shared_from_this(),
                       std::placeholders::_1, std::placeholders::_2));
        start_tunnel(); 
    }
}

void Connection::on_handshake_write_client(boost::system::error_code ec, std::size_t) {
    if (ec) { close(); return; }
    start_tunnel();
}


void Connection::start_tunnel() {
    server_->proxy_pool().report(*current_proxy_, true);
    
    client_socket_.async_read_some(boost::asio::buffer(tunnel_buffer_c2u_),
        std::bind(&Connection::on_tunnel_read_client, shared_from_this(),
                  std::placeholders::_1, std::placeholders::_2));
                  
    upstream_socket_.async_read_some(boost::asio::buffer(tunnel_buffer_u2c_),
        std::bind(&Connection::on_tunnel_read_upstream, shared_from_this(),
                  std::placeholders::_1, std::placeholders::_2));
}

void Connection::on_tunnel_read_client(boost::system::error_code ec, std::size_t length) {
    if (ec) { close(); return; }
    
    boost::asio::async_write(upstream_socket_, boost::asio::buffer(tunnel_buffer_c2u_, length),
        std::bind(&Connection::on_tunnel_write_upstream, shared_from_this(),
                  std::placeholders::_1, std::placeholders::_2));
}

void Connection::on_tunnel_write_upstream(boost::system::error_code ec, std::size_t) {
    if (ec) { close(); return; }
    
    client_socket_.async_read_some(boost::asio::buffer(tunnel_buffer_c2u_),
        std::bind(&Connection::on_tunnel_read_client, shared_from_this(),
                  std::placeholders::_1, std::placeholders::_2));
}

void Connection::on_tunnel_read_upstream(boost::system::error_code ec, std::size_t length) {
    if (ec) { close(); return; }
    
    boost::asio::async_write(client_socket_, boost::asio::buffer(tunnel_buffer_u2c_, length),
        std::bind(&Connection::on_tunnel_write_client, shared_from_this(),
                  std::placeholders::_1, std::placeholders::_2));
}

void Connection::on_tunnel_write_client(boost::system::error_code ec, std::size_t) {
    if (ec) { close(); return; }
    
    
    
    
    
    
    
    
    upstream_socket_.async_read_some(boost::asio::buffer(tunnel_buffer_u2c_),
        std::bind(&Connection::on_tunnel_read_upstream, shared_from_this(),
                  std::placeholders::_1, std::placeholders::_2));
}

}
}
}
