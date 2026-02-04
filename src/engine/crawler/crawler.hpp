#pragma once
#include <atomic>
#include <boost/asio.hpp>
#include <condition_variable>
#include <map>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include "../../browser/launcher/browser_launcher.hpp"
#include "../../core/types/constants.hpp"
#include "../../network/http/http_client.hpp"
#include "../../proxy/pool/proxy_pool.hpp"
#include "../../proxy/server/proxy_server.hpp"
#include "../../storage/disk_storage.hpp"
#include "../../storage/storage.hpp"
#include "../../utils/crypto/bloom_filter.hpp"
#include "../../utils/robotstxt/robotstxt.hpp"
#include "../../utils/url/url.hpp"

namespace Mojo {
namespace Engine {

using namespace Mojo::Core;
using namespace Mojo::Proxy::Pool;
using namespace Mojo::Proxy::Server;
using namespace Mojo::Browser::Launcher;
using namespace Mojo::Network::Http;
using namespace Mojo::Utils;

struct CrawlerConfig {
    int         max_depth       = 0;
    int         threads         = 1;
    int         virtual_threads = 4;
    int         worker_threads  = 1;
    std::string output_dir      = "output";
    bool        tree_structure  = false;
    bool        render_js       = false;
    std::string browser_path;
    bool        headless = true;

    std::string                proxy_bind_ip   = "127.0.0.1";
    int                        proxy_bind_port = 0;
    int                        cdp_port        = 9222;
    std::vector<std::string>   proxies;
    std::map<std::string, int> proxy_priorities;
    int                        proxy_retries         = Mojo::Core::Constants::DEFAULT_PROXY_RETRIES;
    int                        proxy_connect_timeout = 5000;
    int                        proxy_threads         = 32;
    std::string                user_agent            = Mojo::Core::Constants::USER_AGENT;
};

class Crawler {
#ifndef CPPCHECK
    friend class CrawlerTest_ConfigMapping_Test;
    friend class CrawlerTest_AddUrlDepthCheck_Test;
    friend class CrawlerTest_AddUrlDomainCheck_Test;
#endif

public:
    explicit Crawler(const CrawlerConfig& config);
    ~Crawler();
    void start(const std::string& start_url);
    void shutdown();
    void trigger_done();

#ifdef CPPCHECK
public:
#else
private:
#endif
    int         max_depth_;
    int         num_threads_;
    int         num_virtual_threads_;
    int         num_worker_threads_;
    std::string output_dir_;
    bool        tree_structure_;
    bool        use_proxies_;

    std::string proxy_bind_ip_;
    int         proxy_bind_port_;
    int         cdp_port_;

    ProxyPool                    proxy_pool_;
    std::unique_ptr<ProxyServer> proxy_server_;

    std::queue<std::pair<std::string, int>> frontier_;
    BloomFilter                             visited_filter_;

    boost::asio::io_context ioc_;
    std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>
                             work_guard_;
    std::vector<std::thread> io_threads_;
    boost::asio::thread_pool worker_pool_;
    boost::asio::signal_set  signals_{ioc_};

    std::mutex queue_mutex_;

    std::atomic<int>        active_workers_{0};
    std::atomic<int>        pending_content_tasks_{0};
    std::atomic<bool>       done_{false};
    std::condition_variable done_cv_;
    std::mutex              done_mutex_;
    std::atomic<bool>       is_shutdown_{false};
    std::string             start_domain_;
    bool                    render_js_;
    std::string             browser_path_;
    bool                    headless_;
    int                     proxy_connect_timeout_;
    int                     proxy_threads_;
    std::string             user_agent_;

    std::map<std::string, std::shared_ptr<RobotsTxt>> robots_cache_;
    std::mutex                                        robots_mutex_;

    std::map<std::string, std::chrono::steady_clock::time_point> domain_last_access_;
    std::mutex                                                   domain_mutex_;

    void init_io_services();
    void init_signals();
    void init_proxies();
    void init_browser();
    void init_storage();
    void spawn_workers();
    void await_completion();

    std::unique_ptr<Mojo::Storage::Storage> storage_;

    std::unique_ptr<HttpClient>                create_client();
    std::optional<std::pair<std::string, int>> fetch_next_task();
    bool                                       should_stop_worker();

    boost::asio::awaitable<void> process_url_task(HttpClient& client, std::string url, int depth);
    boost::asio::awaitable<bool> check_politeness_and_wait(const std::string& host);

    void process_successful_response(const std::string& url,
                                     int                depth,
                                     Response           res,
                                     const std::string& proxy_url);
    void handle_binary_content(const std::string& url,
                               const std::string& content,
                               const std::string& ext);
    void handle_text_content(const std::string& url, int depth, Response res);
    void parse_and_extract_links(const std::string& base_url, const std::string& html, int depth);

    std::shared_ptr<RobotsTxt> get_cached_robots(const std::string& domain);
    void        cache_robots(const std::string& domain, std::shared_ptr<RobotsTxt> robots);
    static std::string get_robots_url(const Mojo::Utils::UrlParsed& parsed);
    boost::asio::awaitable<std::shared_ptr<RobotsTxt>>
    fetch_robots_txt(const std::string& robots_url, HttpClient& client);
    boost::asio::awaitable<bool> ensure_robots_txt(const Mojo::Utils::UrlParsed& parsed,
                                                   HttpClient&                   client);
    boost::asio::awaitable<bool> is_url_allowed(const std::string& url, HttpClient& client);
    boost::asio::awaitable<bool> wait_for_politeness(const std::string& domain);

    boost::asio::awaitable<void> worker_loop();
    boost::asio::awaitable<bool> fetch_page(HttpClient& client, const std::string& url, int depth);

    void        save_to_storage(const std::string& filename,
                                const std::string& content,
                                bool               is_binary = false);
    std::string get_save_filename(const std::string& url, const std::string& extension = "");

    void add_url(std::string url, int depth);
};

}  // namespace Engine
}  // namespace Mojo
