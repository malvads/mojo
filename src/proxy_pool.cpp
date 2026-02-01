#include "mojo/proxy_pool.hpp"
#include "mojo/logger.hpp"

namespace Mojo {

ProxyPool::ProxyPool(const std::vector<std::string>& proxies) {
    size_t id_counter = 0;
    for (const auto& url : proxies) {
        Proxy p;
        p.url = url;
        p.id = id_counter++;
        
        if (url.find("socks5") != std::string::npos) {
            p.priority = ProxyPriority::SOCKS5;
        } else if (url.find("socks4") != std::string::npos) {
            p.priority = ProxyPriority::SOCKS4;
        } else {
            p.priority = ProxyPriority::HTTP;
        }
        
        queue_.push(p);
    }
    next_id_ = id_counter;
}

std::optional<Proxy> ProxyPool::get_proxy() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty()) return std::nullopt;
    
    Proxy p = queue_.top();
    queue_.pop();
    return p;
}

void ProxyPool::report(Proxy p, bool success) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    p.id = next_id_++;

    if (success) {
        p.failure_count = 0;
        queue_.push(p);
    } else {
        Logger::error("Proxy removed (Failed): " + p.url);
    }
}

bool ProxyPool::empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
}

}
