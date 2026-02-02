#include "crawler.hpp"
#include "../../browser/browser_client.hpp"
#include "../../core/logger/logger.hpp"
#include "../../core/types/constants.hpp"
#include "../../network/http/curl_client.hpp"
#include "../../network/http/http_client.hpp"
#include "../../utils/text/converter.hpp"
#include "../../utils/url/url.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace Mojo {
namespace Engine {

using namespace Mojo::Core;
using namespace Mojo::Network::Http;
using namespace Mojo::Browser;
using namespace Mojo::Utils::Text;
using namespace Mojo::Utils::Url;
using namespace Mojo::Proxy::Server;
using namespace Mojo::Proxy::Pool;
using namespace Mojo::Browser::Launcher;

Crawler::Crawler(const CrawlerConfig& config)
    : max_depth_(config.max_depth),
      num_threads_(config.threads),
      output_dir_(config.output_dir),
      tree_structure_(config.tree_structure),
      use_proxies_(!config.proxies.empty()),
      proxy_bind_ip_(config.proxy_bind_ip),
      proxy_bind_port_(config.proxy_bind_port),
      cdp_port_(config.cdp_port),
      proxy_pool_(config.proxies, config.proxy_retries, config.proxy_priorities),
      render_js_(config.render_js),
      browser_path_(config.browser_path),
      headless_(config.headless),
      proxy_threads_(config.proxy_threads) {
}

Crawler::~Crawler() {
    if (render_js_) {
        BrowserLauncher::cleanup();
    }
}

void Crawler::start(const std::string& start_url) {
    start_domain_ = start_url;
    add_url(start_url, 0);

    for (int i = 0; i < num_threads_; ++i) {
        workers_.emplace_back(&Crawler::worker_loop, this);
    }

    Logger::info("Started " + std::to_string(num_threads_) + " workers.");
    if (!proxy_pool_.empty()) {
        Logger::info("Initialized Proxy Pool.");
        if (render_js_) {
            proxy_server_ = std::make_unique<ProxyServer>(
                proxy_pool_, proxy_bind_ip_, proxy_bind_port_, proxy_threads_);
            proxy_server_->start();
            Logger::info("Local Proxy Gateway started on port "
                         + std::to_string(proxy_server_->get_port()));
        }
    }

    if (render_js_) {
        std::string p_url;
        if (proxy_server_) {
            p_url = proxy_bind_ip_ + ":" + std::to_string(proxy_server_->get_port());
        }

        std::string path = browser_path_;
        if (path.empty())
            path = BrowserLauncher::find_browser();

        if (path.empty()) {
            Logger::error(
                "Could not find a suitable browser (Chrome/Chromium/Edge). Please specify with "
                "--browser.");
            return;
        }

        if (!BrowserLauncher::launch(path, cdp_port_, headless_, p_url)) {
            Logger::error("Failed to launch browser.");
            return;
        }
    }

    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void Crawler::add_url(std::string url, int depth) {
    if (visited_filter_.contains(url)) {
        return;
    }
    visited_filter_.add(url);

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        frontier_.push({std::move(url), depth});
    }
    cv_.notify_one();
}

void Crawler::worker_loop() {
    std::unique_ptr<HttpClient> client;
    if (render_js_) {
        client = std::make_unique<BrowserClient>();
    }
    else {
        client = std::make_unique<CurlClient>();
    }

    while (true) {
        std::pair<std::string, int> task;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            cv_.wait(lock, [this] { return !frontier_.empty() || done_; });

            if (done_ && frontier_.empty()) {
                return;
            }

            task = std::move(frontier_.front());
            frontier_.pop();
            active_workers_++;
        }

        process_task(*client, task.first, task.second);

        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            active_workers_--;
            if (frontier_.empty() && active_workers_ == 0) {
                done_ = true;
                cv_.notify_all();
            }
        }
    }
}

void Crawler::process_task(HttpClient& client, std::string url, int depth) {
    if (fetch_with_retry(client, url, depth)) {
        return;
    }

    if (use_proxies_ && !proxy_pool_.empty()) {
        Logger::warn("Re-queueing URL (Proxy Rotation): " + url);
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            frontier_.push({std::move(url), depth});
        }
        cv_.notify_one();
    }
    else {
        Logger::error("Giving up on URL: " + url);
    }
}

bool Crawler::fetch_with_retry(HttpClient& client, const std::string& url, int depth) {
    if (Url::is_image(url)) {
        Logger::info("Skipping image URL: " + url);
        return true;
    }

    for (int attempt = 1; attempt <= Constants::MAX_RETRIES; ++attempt) {
        auto proxy_opt = proxy_pool_.get_proxy();
        client.set_proxy(proxy_opt ? proxy_opt->url : "");

        std::string log_msg = "Fetching: " + url + " (Depth: " + std::to_string(depth) + ")";
        if (attempt > 1)
            log_msg += " [Retry " + std::to_string(attempt) + "]";
        if (proxy_opt)
            log_msg += " [" + proxy_opt->url + "]";
        Logger::info(log_msg);

        Response res = client.get(url);

        if (res.skipped || res.error_type == Network::Http::ErrorType::Skipped) {
            Logger::info("Skipped (Content-Type Image): " + url);
            return true;
        }

        if (proxy_opt) {
            bool is_proxy_fail = (res.error_type == Network::Http::ErrorType::Proxy
                                  || res.status_code == 403 || res.status_code == 429);
            bool ok            = res.success || (!is_proxy_fail && res.status_code != 0);
            proxy_pool_.report(*proxy_opt, ok);
        }

        bool page_success =
            (res.success || res.status_code == static_cast<long>(HTTPCode::NotFound))
            && res.status_code != 403 && res.status_code != 429;
        if (page_success) {
            handle_response(url, depth, res);
            return true;
        }

        if (attempt == Constants::MAX_RETRIES) {
            std::string err_msg = "Failed: " + url + " (" + res.error + ")";
            if (res.error_type == Network::Http::ErrorType::Render)
                err_msg += " [Render Error]";
            else if (res.error_type == Network::Http::ErrorType::Proxy)
                err_msg += " [Proxy Error]";
            else if (res.error_type == Network::Http::ErrorType::Timeout)
                err_msg += " [Timeout]";
            Logger::error(err_msg + " - Max retries reached");
        }
        else {
            std::this_thread::sleep_for(Mojo::Core::get_backoff_time(attempt));
        }
    }

    return false;
}

void Crawler::handle_response(const std::string& url, int depth, const Response& res) {
    if (res.status_code != static_cast<long>(HTTPCode::Ok)) {
        Logger::warn("HTTP " + std::to_string(res.status_code) + ": " + url);
        return;
    }

    std::string base_url = !res.effective_url.empty() ? res.effective_url : url;

    std::string ext = Mojo::Core::get_file_extension(res.content_type, base_url);

    if (!ext.empty()) {
        save_file(base_url, res.body, ext);
        return;
    }

    std::string markdown = Converter::to_markdown(res.body);
    save_markdown(base_url, markdown);

    if (depth >= max_depth_) {
        return;
    }

    std::vector<std::string> links = Converter::extract_links(res.body);
    for (const auto& link : links) {
        std::string absolute_link = Url::resolve(base_url, link);
        if (absolute_link.empty())
            continue;
        if (!Url::is_same_domain(start_domain_, absolute_link))
            continue;

        add_url(std::move(absolute_link), depth + 1);
    }
}

void Crawler::save_markdown(const std::string& url, const std::string& content) {
    std::string filename;
    if (tree_structure_) {
        filename = Url::to_filename(url);
    }
    else {
        filename = Url::to_flat_filename(url);
    }

    try {
        std::filesystem::path path(output_dir_);
        path /= filename;

        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path());
        }

        std::ofstream file(path);
        if (file.is_open()) {
            file << content;
            file.close();
            Logger::success("Saved: " + path.string());
        }
        else {
            Logger::error("Failed to write to file: " + path.string());
        }
    } catch (...) {
        Logger::error("FS Error: " + filename);
    }
}

void Crawler::save_file(const std::string& url,
                        const std::string& content,
                        const std::string& extension) {
    std::string filename;
    if (tree_structure_) {
        filename = Url::to_filename(url);
    }
    else {
        filename = Url::to_flat_filename(url);
    }

    if (filename.size() > 3 && filename.substr(filename.size() - 3) == ".md") {
        filename = filename.substr(0, filename.size() - 3) + extension;
    }

    try {
        std::filesystem::path path(output_dir_);
        path /= filename;

        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path());
        }

        std::ofstream file(path, std::ios::binary);
        if (file.is_open()) {
            file.write(content.data(), content.size());
            file.close();
            Logger::success("Downloaded: " + path.string());
        }
        else {
            Logger::error("Failed to write to file: " + path.string());
        }
    } catch (...) {
        Logger::error("FS Error: " + filename);
    }
}

}  // namespace Engine
}  // namespace Mojo
