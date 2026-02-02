#include "proxy_pool.hpp"
#include <algorithm>
#include <limits>
#include "logger/logger.hpp"

namespace Mojo {
namespace Proxy {
namespace Pool {

using namespace Mojo::Core;

ProxyPool::ProxyPool(const std::vector<std::string>&   proxies,
                     int                               max_retries,
                     const std::map<std::string, int>& priorities)
    : max_retries_(max_retries) {
    size_t id_counter = 0;
    for (const auto& url : proxies) {
        Proxy p;
        p.url      = url;
        p.id       = id_counter++;
        p.priority = determine_priority(url, priorities);
        proxies_.push_back(p);
    }
}

ProxyPriority ProxyPool::determine_priority(const std::string&                url,
                                            const std::map<std::string, int>& priorities) const {
    if (url.find("socks5") != std::string::npos) {
        return static_cast<ProxyPriority>(priorities.at("socks5"));
    }
    if (url.find("socks4") != std::string::npos) {
        return static_cast<ProxyPriority>(priorities.at("socks4"));
    }
    auto it = priorities.find("http");
    return (it != priorities.end()) ? static_cast<ProxyPriority>(it->second) : ProxyPriority::HTTP;
}

std::optional<Proxy> ProxyPool::get_proxy() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (proxies_.empty())
        return std::nullopt;

    ProxyPriority highest_p = ProxyPriority::HTTP;
    for (const auto& p : proxies_) {
        if (p.priority > highest_p)
            highest_p = p.priority;
    }

    int min_failures = std::numeric_limits<int>::max();
    for (const auto& p : proxies_) {
        if (p.priority == highest_p && p.failure_count < min_failures) {
            min_failures = p.failure_count;
        }
    }

    std::vector<size_t> candidates;
    for (size_t i = 0; i < proxies_.size(); ++i) {
        if (proxies_[i].priority == highest_p && proxies_[i].failure_count == min_failures) {
            candidates.push_back(i);
        }
    }

    if (candidates.empty())
        return std::nullopt;

    size_t last_idx          = last_idx_map_.count(highest_p) ? last_idx_map_[highest_p] : 0;
    size_t selected_cand_idx = 0;
    for (size_t i = 0; i < candidates.size(); ++i) {
        if (candidates[i] > last_idx) {
            selected_cand_idx = i;
            break;
        }
    }

    size_t final_idx         = candidates[selected_cand_idx];
    last_idx_map_[highest_p] = final_idx;

    return proxies_[final_idx];
}

void ProxyPool::report(Proxy p, bool success) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = std::find_if(
        proxies_.begin(), proxies_.end(), [&](const Proxy& sp) { return sp.url == p.url; });

    if (it != proxies_.end()) {
        if (success) {
            it->failure_count = 0;
        }
        else {
            it->failure_count++;
            if (it->failure_count <= max_retries_) {
                Logger::warn("Proxy failed (" + std::to_string(it->failure_count) + "/"
                             + std::to_string(max_retries_) + "): " + it->url);
            }
            else {
                Logger::error("Proxy removed (Max Retries Exceeded): " + it->url);
            }
        }
    }

    proxies_.erase(std::remove_if(proxies_.begin(),
                                  proxies_.end(),
                                  [&](const Proxy& sp) { return sp.failure_count > max_retries_; }),
                   proxies_.end());
}

bool ProxyPool::empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return proxies_.empty();
}

}  // namespace Pool
}  // namespace Proxy
}  // namespace Mojo
