#pragma once
#include <atomic>
#include <condition_variable>
#include <map>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include "../../browser/launcher/browser_launcher.hpp"
#include "../../network/http/http_client.hpp"
#include "../../proxy/pool/proxy_pool.hpp"
#include "../../proxy/server/proxy_server.hpp"
#include "../../utils/crypto/bloom_filter.hpp"

namespace Mojo {
namespace Engine {

using namespace Mojo::Core;
using namespace Mojo::Proxy::Pool;
using namespace Mojo::Proxy::Server;
using namespace Mojo::Browser::Launcher;
using namespace Mojo::Network::Http;

struct CrawlerConfig {
    int         max_depth;
    int         threads;
    std::string output_dir;
    bool        tree_structure;
    bool        render_js = false;
    std::string browser_path;
    bool        headless = true;

    std::string                proxy_bind_ip   = "127.0.0.1";
    int                        proxy_bind_port = 0;
    int                        cdp_port        = 9222;
    std::vector<std::string>   proxies;
    std::map<std::string, int> proxy_priorities;
    int                        proxy_retries;
    int                        proxy_threads = 32;
};

class Crawler {
    friend class CrawlerTest;

public:
    explicit Crawler(const CrawlerConfig& config);
    ~Crawler();
    void start(const std::string& start_url);

private:
    int         max_depth_;
    int         num_threads_;
    std::string output_dir_;
    bool        tree_structure_;
    bool        use_proxies_;

    std::string proxy_bind_ip_;
    int         proxy_bind_port_;
    int         cdp_port_;

    ProxyPool                    proxy_pool_;
    std::unique_ptr<ProxyServer> proxy_server_;

    std::vector<std::thread>                workers_;
    std::queue<std::pair<std::string, int>> frontier_;
    BloomFilter                             visited_filter_;

    std::mutex              queue_mutex_;
    std::condition_variable cv_;

    std::atomic<int>  active_workers_{0};
    std::atomic<bool> done_{false};
    std::string       start_domain_;
    bool              render_js_;
    std::string       browser_path_;
    bool              headless_;
    int               proxy_threads_;

    void worker_loop();
    void process_task(HttpClient& client, std::string url, int depth);
    bool fetch_with_retry(HttpClient& client, const std::string& url, int depth);
    void handle_response(const std::string& url, int depth, const Response& res);
    void save_markdown(const std::string& url, const std::string& content);
    void
    save_file(const std::string& url, const std::string& content, const std::string& extension);
    void add_url(std::string url, int depth);
};

}  // namespace Engine
}  // namespace Mojo
