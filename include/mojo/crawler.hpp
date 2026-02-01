#pragma once
#include <string>
#include <unordered_set>
#include <queue>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include "mojo/proxy_pool.hpp"
#include "mojo/bloom_filter.hpp"
#include "mojo/http_client.hpp"

namespace Mojo {

class Crawler {
public:
    Crawler(int max_depth, int threads, 
            std::string output_dir, bool tree_structure,
            bool render_js = false,
            const std::vector<std::string>& proxies = {});
    void start(const std::string& start_url);

private:
    int max_depth_;
    int num_threads_;
    std::string output_dir_;
    bool tree_structure_;
    bool use_proxies_;
    ProxyPool proxy_pool_;
    
    std::vector<std::thread> workers_;
    std::queue<std::pair<std::string, int>> frontier_;
    BloomFilter visited_filter_;
    
    std::mutex queue_mutex_;
    std::condition_variable cv_;
    
    std::atomic<int> active_workers_{0};
    std::atomic<bool> done_{false};
    std::string start_domain_;
    bool render_js_;

    void worker_loop();
    void process_task(HttpClient& client, std::string url, int depth);
    bool fetch_with_retry(HttpClient& client, const std::string& url, int depth);
    void handle_response(const std::string& url, int depth, const Response& res);
    void save_markdown(const std::string& url, const std::string& content);
    void save_file(const std::string& url, const std::string& content, const std::string& extension);
    void add_url(std::string url, int depth);
};

}
