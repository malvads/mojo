#pragma once
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <optional>
#include <atomic>
#include <map>

namespace Mojo {

enum class ProxyPriority {
    HTTP = 0,
    SOCKS4 = 1,
    SOCKS5 = 2
};

struct Proxy {
    std::string url;
    int failure_count = 0;
    ProxyPriority priority = ProxyPriority::HTTP;
    size_t id = 0;    // Stable ordering tie-breaker

    // Priority Queue is Max-Heap. We want higher priority at top.
    // If priorities are equal, prefer SMALLER id (older/FIFO).
    // Max-Heap pops LARGEST element.
    // So if we want SMALLER id to be popped, SMALLER id must be "LARGER" in comparison.
    // Return true if `this` is "smaller" (lower priority) than `other`.
    bool operator<(const Proxy& other) const {
        if (priority != other.priority) return priority < other.priority;
        return id > other.id; // Larger ID -> "Smaller value" -> Lower in heap -> Popped last.
    }
};

class ProxyPool {
public:
    explicit ProxyPool(const std::vector<std::string>& proxies, int max_retries, const std::map<std::string, int>& priorities);

    std::optional<Proxy> get_proxy();
    void report(Proxy p, bool success);
    bool empty() const;

private:
    ProxyPriority determine_priority(const std::string& url, const std::map<std::string, int>& priorities) const;
    std::vector<Proxy> proxies_;
    mutable std::mutex mutex_;
    int max_retries_;
    std::map<ProxyPriority, size_t> last_idx_map_;
};

}
