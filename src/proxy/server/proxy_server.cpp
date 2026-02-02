#include "proxy_server.hpp"
#include "connection.hpp"
#include "../../core/logger/logger.hpp"

namespace Mojo {
namespace Proxy {
namespace Server {

using namespace Mojo::Core;
using Pool::ProxyPool;

ProxyServer::ProxyServer(ProxyPool& proxy_pool, const std::string& bind_ip, int bind_port, int thread_count)
    : proxy_pool_(proxy_pool), bind_ip_(bind_ip), bind_port_(bind_port), thread_count_(thread_count),
      acceptor_(io_context_) {
}

ProxyServer::~ProxyServer() {
    stop();
}

void ProxyServer::start() {
    try {
        boost::asio::ip::tcp::resolver resolver(io_context_);
        boost::asio::ip::tcp::endpoint endpoint = *resolver.resolve(bind_ip_, std::to_string(bind_port_)).begin();
        
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
        acceptor_.bind(endpoint);
        acceptor_.listen();
        
        port_ = acceptor_.local_endpoint().port();
        Logger::info("ProxyServer: Async (Asio) Listening on " + bind_ip_ + ":" + std::to_string(port_));

        do_accept();

        for (int i = 0; i < thread_count_; ++i) {
            threads_.emplace_back([this]() {
                io_context_.run();
            });
        }
    } catch (std::exception& e) {
        Logger::error("ProxyServer: Exception: " + std::string(e.what()));
    }
}

void ProxyServer::stop() {
    if (!io_context_.stopped()) {
        io_context_.stop();
    }
    for (auto& t : threads_) {
        if (t.joinable()) t.join();
    }
    threads_.clear();
}

int ProxyServer::get_port() const {
    return port_;
}

void ProxyServer::do_accept() {
    acceptor_.async_accept([this](boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
        if (!ec) {
            std::make_shared<Connection>(std::move(socket), this)->start();
        } else {
            Logger::error("ProxyServer: Accept error: " + ec.message());
        }

        do_accept();
    });
}

}
}
}
