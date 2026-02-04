#include <boost/asio/steady_timer.hpp>
#include "../../../core/logger/logger.hpp"
#include "../crawler.hpp"

namespace Mojo {
namespace Engine {

namespace {
constexpr int    POLITENESS_BUFFER_MS = 100;
constexpr double MIN_DELAY            = 0.0;
}  // namespace

std::shared_ptr<RobotsTxt> Crawler::get_cached_robots(const std::string& domain) {
    std::lock_guard<std::mutex> lock(robots_mutex_);
    auto                        it = robots_cache_.find(domain);
    return (it != robots_cache_.end()) ? it->second : nullptr;
}

void Crawler::cache_robots(const std::string& domain, std::shared_ptr<RobotsTxt> robots) {
    std::lock_guard<std::mutex> lock(robots_mutex_);
    robots_cache_[domain] = robots;
}

std::string Crawler::get_robots_url(const Mojo::Utils::UrlParsed& parsed) {
    std::string proto = parsed.scheme + "://" + parsed.host;
    if (!parsed.port.empty())
        proto += ":" + parsed.port;
    return proto + "/robots.txt";
}

boost::asio::awaitable<std::shared_ptr<RobotsTxt>>
Crawler::fetch_robots_txt(const std::string& robots_url, HttpClient& client) {
    Logger::info("Fetching robots.txt: " + robots_url);
    Response res = co_await client.get(robots_url);

    if (res.status_code >= 200 && res.status_code < 300) {
        Logger::info("Parsed robots.txt");
        co_return std::make_shared<RobotsTxt>(RobotsTxt::parse(res.body));
    }
    Logger::warn("Missing robots.txt (" + std::to_string(res.status_code) + ")");
    co_return std::make_shared<RobotsTxt>();
}

boost::asio::awaitable<bool> Crawler::ensure_robots_txt(const Mojo::Utils::UrlParsed& parsed,
                                                        HttpClient&                   client) {
    if (get_cached_robots(parsed.host))
        co_return true;
    auto robots = co_await fetch_robots_txt(get_robots_url(parsed), client);
    cache_robots(parsed.host, robots);
    co_return true;
}

boost::asio::awaitable<bool> Crawler::is_url_allowed(const std::string& url, HttpClient& client) {
    auto parsed = Mojo::Utils::Url::parse(url);
    if (parsed.host.empty() || parsed.path == "/robots.txt")
        co_return true;

    co_await ensure_robots_txt(parsed, client);
    auto     robots = get_cached_robots(parsed.host);

    if (robots && !robots->is_allowed(user_agent_, parsed.path.empty() ? "/" : parsed.path)) {
        Logger::info("Blocked by robots.txt: " + url);
        co_return false;
    }
    co_return true;
}

boost::asio::awaitable<bool> Crawler::check_politeness_and_wait(const std::string& host) {
    return wait_for_politeness(host);
}

boost::asio::awaitable<bool> Crawler::wait_for_politeness(const std::string& domain) {
    auto   robots = get_cached_robots(domain);
    double delay  = robots ? robots->get_crawl_delay(user_agent_) : MIN_DELAY;
    if (delay <= MIN_DELAY)
        co_return true;

    std::chrono::milliseconds delay_ms(static_cast<long long>(delay * 1000));
    std::chrono::milliseconds wait_time(0);

    {
        std::lock_guard<std::mutex> lock(domain_mutex_);
        auto                        it  = domain_last_access_.find(domain);
        auto                        now = std::chrono::steady_clock::now();

        if (it != domain_last_access_.end()) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second);
            if (elapsed < delay_ms)
                wait_time = delay_ms - elapsed;
        }

        if (wait_time > std::chrono::milliseconds(POLITENESS_BUFFER_MS))
            co_return false;

        domain_last_access_[domain] = now + wait_time;
    }

    if (wait_time.count() > 0) {
        Logger::info("Politeness: Waiting " + std::to_string(wait_time.count()) + "ms for "
                     + domain);
        boost::asio::steady_timer timer(ioc_);
        timer.expires_after(wait_time);
        co_await timer.async_wait(boost::asio::use_awaitable);
    }

    co_return true;
}

}  // namespace Engine
}  // namespace Mojo
