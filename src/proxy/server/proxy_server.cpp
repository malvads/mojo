#include "proxy_server.hpp"
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include "../../core/logger/logger.hpp"
#include "connection.hpp"

namespace Mojo {
namespace Proxy {
namespace Server {

using namespace Mojo::Core;
using Pool::ProxyPool;

ProxyServer::ProxyServer(ProxyPool&         proxy_pool,
                         const std::string& bind_ip,
                         int                bind_port,
                         int                thread_count)
    : proxy_pool_(proxy_pool),
      bind_ip_(bind_ip),
      bind_port_(bind_port),
      thread_count_(thread_count),
      acceptor_(io_context_) {
}

ProxyServer::~ProxyServer() {
    stop();
}

void ProxyServer::start() {
    try {
        boost::asio::ip::tcp::resolver resolver(io_context_);
        boost::asio::ip::tcp::endpoint endpoint =
            *resolver.resolve(bind_ip_, std::to_string(bind_port_)).begin();

        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
        acceptor_.bind(endpoint);
        acceptor_.listen();

        port_ = acceptor_.local_endpoint().port();
        Logger::info("ProxyServer: Async (Asio/Coroutines) Listening on " + bind_ip_ + ":"
                     + std::to_string(port_));

        boost::asio::co_spawn(io_context_, do_accept(), boost::asio::detached);

        for (int i = 0; i < thread_count_; ++i) {
            threads_.emplace_back([this]() { io_context_.run(); });
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
        if (t.joinable())
            t.join();
    }
    threads_.clear();
}

int ProxyServer::get_port() const {
    return port_;
}

boost::asio::awaitable<void> ProxyServer::do_accept() {
    while (true) {
        try {
            auto socket = co_await acceptor_.async_accept(boost::asio::use_awaitable);
            std::make_shared<Connection>(std::move(socket), this)->start();
        } catch (const std::exception& e) {
            Logger::error("ProxyServer: Accept error: " + std::string(e.what()));
        }
    }
}

}  // namespace Server
}  // namespace Proxy
}  // namespace Mojo
