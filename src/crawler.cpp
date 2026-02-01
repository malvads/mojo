#include "mojo/crawler.hpp"
#include "mojo/client.hpp"
#include "mojo/converter.hpp"
#include "mojo/logger.hpp"
#include "mojo/url.hpp"
#include "mojo/constants.hpp"

#include <iostream>
#include <fstream>
#include <filesystem>

namespace Mojo {

Crawler::Crawler(int max_depth, int threads, 
                 std::string output_dir, bool tree_structure,
                 const std::vector<std::string>& proxies) 
    : max_depth_(max_depth), num_threads_(threads), 
      output_dir_(std::move(output_dir)), tree_structure_(tree_structure),
      use_proxies_(!proxies.empty()),
      proxy_pool_(proxies) {}

void Crawler::start(const std::string& start_url) {
    start_domain_ = start_url;
    add_url(start_url, 0);

    for (int i = 0; i < num_threads_; ++i) {
        workers_.emplace_back(&Crawler::worker_loop, this);
    }
    
    Logger::info("Started " + std::to_string(num_threads_) + " workers.");
    if (!proxy_pool_.empty()) {
        Logger::info("Initialized Proxy Pool.");
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
    Client client;
    
    while (true) {
        std::pair<std::string, int> task;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            cv_.wait(lock, [this] { 
                return !frontier_.empty() || done_; 
            });

            if (done_ && frontier_.empty()) {
                return;
            }

            task = std::move(frontier_.front());
            frontier_.pop();
            active_workers_++;
        }

        process_task(client, task.first, task.second);

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

void Crawler::process_task(Client& client, std::string url, int depth) {
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
    } else {
        Logger::error("Giving up on URL: " + url);
    }
}

bool Crawler::fetch_with_retry(Client& client, const std::string& url, int depth) {
    if (Url::is_image(url)) {
        Logger::info("Skipping image URL: " + url);
        return true;
    }

    for (int attempt = 1; attempt <= Constants::MAX_RETRIES; ++attempt) {
        auto proxy_opt = proxy_pool_.get_proxy();
        client.set_proxy(proxy_opt ? proxy_opt->url : "");

        std::string log_msg = "Fetching: " + url + " (Depth: " + std::to_string(depth) + ")";
        if (attempt > 1) log_msg += " [Retry " + std::to_string(attempt) + "]";
        if (proxy_opt) log_msg += " [" + proxy_opt->url + "]";
        Logger::info(log_msg);

        Response res = client.get(url);

        // Aborted due to image Content-Type
        if (!res.success && res.error == "Skipped: Image detected") {
            Logger::info("Skipped (Content-Type Image): " + url);
            return true;
        }

        if (proxy_opt) {
            bool ok = res.success || (res.status_code != 0 && res.status_code != 403 && res.status_code != 429);
            proxy_pool_.report(*proxy_opt, ok);
        }

        bool page_success = (res.success || res.status_code == 404) && res.status_code != 403 && res.status_code != 429;
        if (page_success) {
            handle_response(url, depth, res);
            return true;
        }

        if (attempt == Constants::MAX_RETRIES) {
            Logger::error("Failed: " + url + " (" + res.error + ") - Max retries reached");
        }
    }

    return false;
}

void Crawler::handle_response(const std::string& url, int depth, const Response& res) {
    if (res.status_code != 200) {
        Logger::warn("HTTP " + std::to_string(res.status_code) + ": " + url);
        return;
    }

    std::string base_url = !res.effective_url.empty() ? res.effective_url : url;

    if (res.content_type == "application/pdf" || base_url.substr(base_url.length() - 4) == ".pdf") {
        save_file(base_url, res.body, ".pdf");
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
        if (absolute_link.empty()) continue;
        if (!Url::is_same_domain(start_domain_, absolute_link)) continue;
        
        add_url(std::move(absolute_link), depth + 1);
    }
}


void Crawler::save_markdown(const std::string& url, const std::string& content) {
    std::string filename;
    if (tree_structure_) {
        filename = Url::to_filename(url);
    } else {
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
        } else {
            Logger::error("Failed to write to file: " + path.string());
        }
    } catch (...) {
        Logger::error("FS Error: " + filename);
    }
}

void Crawler::save_file(const std::string& url, const std::string& content, const std::string& extension) {
    std::string filename;
    if (tree_structure_) {
        filename = Url::to_filename(url);
    } else {
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
        } else {
            Logger::error("Failed to write to file: " + path.string());
        }
    } catch (...) {
        Logger::error("FS Error: " + filename);
    }
}

}
