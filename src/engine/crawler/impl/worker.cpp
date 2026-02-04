#include <boost/asio/co_spawn.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <iostream>
#include "../../../browser/browser_client.hpp"
#include "../../../core/logger/logger.hpp"
#include "../../../core/types/constants.hpp"
#include "../../../network/http/beast_client.hpp"
#include "../../../utils/text/converter.hpp"
#include "../../../utils/url/url.hpp"
#include "../crawler.hpp"

namespace Mojo {
namespace Engine {

using namespace Mojo::Core;
using namespace Mojo::Network::Http;
using namespace Mojo::Browser;
using namespace Mojo::Utils::Text;

namespace {
constexpr int WORKER_POLL_INTERVAL_MS = 50;
constexpr int REQUEUE_DELAY_MS        = 1000;
}  // namespace

void Crawler::add_url(std::string url, int depth) {
    if (depth > max_depth_)
        return;

    if (!start_domain_.empty()) {
        auto parsed = Mojo::Utils::Url::parse(url);
        if (parsed.host != start_domain_) {
            return;
        }
    }

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (visited_filter_.contains(url))
            return;
        visited_filter_.add(url);
        frontier_.push({std::move(url), depth});
    }
}

std::unique_ptr<HttpClient> Crawler::create_client() {
    if (render_js_) {
        return std::make_unique<BrowserClient>(ioc_);
    }
    auto client = std::make_unique<BeastClient>(ioc_);
    client->set_connect_timeout(std::chrono::milliseconds(proxy_connect_timeout_));
    return client;
}

std::optional<std::pair<std::string, int>> Crawler::fetch_next_task() {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    if (frontier_.empty())
        return std::nullopt;

    auto task = std::move(frontier_.front());
    frontier_.pop();
    active_workers_++;
    return task;
}

bool Crawler::should_stop_worker() {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return done_ || (active_workers_ == 0 && pending_content_tasks_ == 0 && frontier_.empty());
}

struct WorkerGuard {
    std::atomic<int>& count;
    explicit WorkerGuard(std::atomic<int>& c) : count(c) {
    }
    ~WorkerGuard() {
        count--;
    }
};

boost::asio::awaitable<void> Crawler::worker_loop() {
    try {
        auto                      client = create_client();
        boost::asio::steady_timer timer(ioc_);

        while (!done_) {
            auto task_opt = fetch_next_task();

            if (!task_opt) {
                if (should_stop_worker()) {
                    if (!done_.exchange(true)) {
                        trigger_done();
                    }
                    co_return;
                }
                timer.expires_after(std::chrono::milliseconds(WORKER_POLL_INTERVAL_MS));
                boost::system::error_code ec;
                co_await                  timer.async_wait(
                    boost::asio::redirect_error(boost::asio::use_awaitable, ec));
                if (done_) {
                    co_return;
                }
                continue;
            }

            {
                WorkerGuard guard(active_workers_);
                co_await    process_url_task(*client, task_opt->first, task_opt->second);
            }
        }
    } catch (const std::exception& e) {
        Logger::error("Worker Loop Exception: " + std::string(e.what()));
    } catch (...) {
        Logger::error("Worker Loop Unknown Exception - possible memory corruption or system error");
    }
}

boost::asio::awaitable<void>
Crawler::process_url_task(HttpClient& client, std::string url, int depth) {
    if (!co_await is_url_allowed(url, client))
        co_return;

    auto parsed = Mojo::Utils::Url::parse(url);
    if (!parsed.host.empty()) {
        if (!co_await check_politeness_and_wait(parsed.host)) {
            boost::asio::steady_timer timer(ioc_);
            timer.expires_after(std::chrono::milliseconds(REQUEUE_DELAY_MS));
            co_await timer.async_wait(boost::asio::use_awaitable);

            std::lock_guard<std::mutex> lock(queue_mutex_);
            frontier_.push({url, depth});
            co_return;
        }
    }

    if (co_await fetch_page(client, url, depth))
        co_return;

    if (use_proxies_ && !proxy_pool_.empty()) {
        Logger::warn("Re-queueing (Rotation): " + url);
        std::lock_guard<std::mutex> lock(queue_mutex_);
        frontier_.push({std::move(url), depth});
    } else {
        Logger::error("Giving up: " + url);
    }
    co_return;
}

boost::asio::awaitable<bool>
Crawler::fetch_page(HttpClient& client, const std::string& url, int depth) {
    if (Mojo::Utils::Url::is_image(url)) {
        Logger::info("Skipping image: " + url);
        co_return true;
    }

    for (int attempt = 1; attempt <= Constants::MAX_RETRIES; ++attempt) {
        auto proxy_opt = proxy_pool_.get_proxy();
        client.set_proxy(proxy_opt ? proxy_opt->url : "");

        std::string log_msg = "Fetching: " + url + " (Depth " + std::to_string(depth) + ")";
        if (attempt > 1)
            log_msg += " [Retry " + std::to_string(attempt) + "]";
        if (proxy_opt)
            log_msg += " [" + proxy_opt->url + "]";
        Logger::info(log_msg);

        Response res = co_await client.get(url);

        if (res.skipped || res.error_type == ErrorType::Skipped) {
            Logger::info("Skipped (Type): " + url);
            co_return true;
        }

        if (proxy_opt) {
            bool is_proxy_fail = (res.error_type == ErrorType::Proxy || res.status_code == 403
                                  || res.status_code == 429);
            proxy_pool_.report(*proxy_opt, res.success || (!is_proxy_fail && res.status_code != 0));
        }

        bool success = (res.success || res.status_code == static_cast<long>(HTTPCode::NotFound))
                       && res.status_code != 403 && res.status_code != 429;

        if (success) {
            process_successful_response(
                url, depth, std::move(res), proxy_opt ? proxy_opt->url : "");
            co_return true;
        }

        if (attempt == Constants::MAX_RETRIES) {
            std::string suffix = proxy_opt ? " [" + proxy_opt->url + "]" : "";
            Logger::error("Failed: " + url + " - Max retries" + suffix);
        }
        else {
            boost::asio::steady_timer timer(ioc_);
            timer.expires_after(Mojo::Core::get_backoff_time(attempt));
            co_await timer.async_wait(boost::asio::use_awaitable);
        }
    }
    co_return false;
}

void Crawler::process_successful_response(const std::string& url,
                                          int                depth,
                                          Response           res,
                                          const std::string& proxy_url) {
    if (res.status_code != static_cast<long>(HTTPCode::Ok)) {
        std::string suffix = proxy_url.empty() ? "" : " [" + proxy_url + "]";
        Logger::warn("HTTP " + std::to_string(res.status_code) + ": " + url + suffix);
        return;
    }

    std::string base_url = !res.effective_url.empty() ? res.effective_url : url;
    std::string ext      = Mojo::Core::get_file_extension(res.content_type, base_url);

    if (!ext.empty()) {
        handle_binary_content(base_url, res.body, ext);
    }
    else {
        handle_text_content(base_url, depth, std::move(res));
    }
}

void Crawler::handle_text_content(const std::string& url, int depth, Response res) {
    pending_content_tasks_++;
    boost::asio::post(worker_pool_, [this, url, content = std::move(res.body), depth]() {
        try {
            std::string markdown = Converter::to_markdown(content);
            save_to_storage(get_save_filename(url), markdown);

            if (depth < max_depth_) {
                parse_and_extract_links(url, content, depth);
            }
        } catch (const std::exception& e) {
            Logger::error("Content processing failed for " + url + ": " + std::string(e.what()));
        } catch (...) {
            Logger::error("Content processing unknown error for " + url
                          + " - possible memory corruption");
        }
        pending_content_tasks_--;
    });
}

void Crawler::parse_and_extract_links(const std::string& base_url,
                                      const std::string& html,
                                      int                depth) {
    std::vector<std::string> links = Converter::extract_links(html);
    for (const auto& link : links) {
        std::string absolute_link = Mojo::Utils::Url::resolve(base_url, link);
        if (absolute_link.empty())
            continue;
        add_url(std::move(absolute_link), depth + 1);
    }
}

}  // namespace Engine
}  // namespace Mojo
