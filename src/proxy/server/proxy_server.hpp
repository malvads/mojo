#pragma once

#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <boost/asio.hpp>
#include "../pool/proxy_pool.hpp"

namespace Mojo {
namespace Proxy {
namespace Server {

// Forward declare or use from Pool
using Pool::ProxyPool;

class Connection; 

class ProxyServer {
public:
    ProxyServer(ProxyPool& proxy_pool, const std::string& bind_ip, int bind_port, int thread_count);
    ~ProxyServer();

    void start();
    void stop();
    int get_port() const;
    
    ProxyPool& proxy_pool() { return proxy_pool_; }
    boost::asio::io_context& io_context() { return io_context_; }

private:
    void do_accept();

    ProxyPool& proxy_pool_;
    std::string bind_ip_;
    int bind_port_;
    int thread_count_;
    int port_ = 0;
    
    boost::asio::io_context io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::vector<std::thread> threads_;
};

}
}
}
