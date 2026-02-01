#pragma once
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <optional>
#include <atomic>

namespace Mojo {

struct Proxy {
    std::string url;
    int failure_count = 0;
    int priority = 0; // 2=SOCKS5, 1=SOCKS4, 0=HTTP/HTTPS
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
    explicit ProxyPool(const std::vector<std::string>& proxies);

    std::optional<Proxy> get_proxy();
    void report(Proxy p, bool success);
    bool empty() const;

private:
    std::priority_queue<Proxy> queue_;
    mutable std::mutex mutex_;
    size_t next_id_ = 0;
};

}
