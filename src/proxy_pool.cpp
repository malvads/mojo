#include "mojo/proxy_pool.hpp"
#include "mojo/logger.hpp"

namespace Mojo {

ProxyPool::ProxyPool(const std::vector<std::string>& proxies, int max_retries, const std::map<std::string, int>& priorities) 
    : max_retries_(max_retries) {
    size_t id_counter = 0;
    for (const auto& url : proxies) {
        Proxy p;
        p.url = url;
        p.id = id_counter++;
        
        if (url.find("socks5") != std::string::npos) {
            p.priority = static_cast<ProxyPriority>(priorities.at("socks5"));
        } else if (url.find("socks4") != std::string::npos) {
            p.priority = static_cast<ProxyPriority>(priorities.at("socks4"));
        } else {
             // Fallback/Default to finding "http" or using 0 if not found, 
             // but since we initialize the map in Config with defaults, .at("http") should work.
             // To be safe against user removing keys from YAML, we should use find() or [] if const issue.
             // But map is passed const. 
             if (priorities.count("http")) {
                 p.priority = static_cast<ProxyPriority>(priorities.at("http"));
             } else {
                 p.priority = ProxyPriority::HTTP; 
             }
        }
        
        proxies_.push_back(p);
    }
}

std::optional<Proxy> ProxyPool::get_proxy() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (proxies_.empty()) return std::nullopt;
    
    // Find best proxy: Highest Priority -> Lowest Failure Count
    const Proxy* best = nullptr;
    
    for (const auto& p : proxies_) {
        if (!best) {
            best = &p;
            continue;
        }
        
        // Higher Priority is better
        if (p.priority > best->priority) {
            best = &p;
            continue;
        } else if (p.priority < best->priority) {
            continue;
        }
        
        // Same Priority: Lower Failure Count is better
        if (p.failure_count < best->failure_count) {
            best = &p;
            continue;
        }
    }
    
    if (!best) return std::nullopt;
    
    // Return a copy. We do NOT remove it from the list.
    return *best; 
}

void ProxyPool::report(Proxy p, bool success) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& stored_proxy : proxies_) {
        if (stored_proxy.url == p.url) { // Match by URL (assuming unique URLs)
            if (success) {
                stored_proxy.failure_count = 0;
            } else {
                stored_proxy.failure_count++;
                if (stored_proxy.failure_count <= max_retries_) {
                    Logger::warn("Proxy failed (" + std::to_string(stored_proxy.failure_count) + "/" + std::to_string(max_retries_) + "): " + stored_proxy.url);
                } else {
                    Logger::error("Proxy removed (Max Retries Exceeded): " + stored_proxy.url);
                     // Remove from vector
                     // Note: Handled by removing from vector, but iterating + erasing is tricky.
                     // Easier: Swap with back and pop_back
                     // We need to find the iterator.
                }
            }
            break;
        }
    }

    // Cleanup Loop for max retries
    // We do this separately or carefully inside
    for (auto it = proxies_.begin(); it != proxies_.end(); ) {
        if (it->failure_count > max_retries_) {
             it = proxies_.erase(it);
        } else {
             ++it;
        }
    }
}

bool ProxyPool::empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return proxies_.empty();
}

}
