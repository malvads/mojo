#include "mojo/crawler.hpp"
#include "mojo/client.hpp"
#include "mojo/converter.hpp"
#include "mojo/logger.hpp"
#include "mojo/url.hpp"

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
    {
        std::lock_guard<std::mutex> lock(visited_mutex_);
        if (visited_.find(url) != visited_.end()) {
            return;
        }
        visited_.insert(url);
    }
    
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

        auto [current_url, depth] = task;
        bool fetch_success = false;
        
        // Retry Loop (Max 3 attempts)
        for (int attempt = 1; attempt <= 3; ++attempt) {
            // Optimization: Check extension before fetching
            if (Url::is_image(current_url)) {
                Logger::info("Skipping image URL: " + current_url);
                fetch_success = true;
                break;
            }

            auto proxy_opt = proxy_pool_.get_proxy();
            if (proxy_opt) {
                client.set_proxy(proxy_opt->url);
            } else {
                 client.set_proxy(""); // Direct if no proxies
            }

            std::string log_msg = "Fetching: " + current_url + " (Depth: " + std::to_string(depth) + ")";
            if (attempt > 1) log_msg += " [Retry " + std::to_string(attempt) + "]";
            if (proxy_opt) log_msg += " [" + proxy_opt->url + "]";
            
            Logger::info(log_msg);
            
            Response res = client.get(current_url);
            
            // Check if aborted due to image content type
            if (!res.success && res.error == "Skipped: Image detected") {
                Logger::info("Skipped (Content-Type Image): " + current_url);
                fetch_success = true;
                break;
            }
            
            if (proxy_opt) {
                bool ok = true;
                if (!res.success && res.status_code == 0) ok = false; // Connection error
                if (res.status_code == 403 || res.status_code == 429) ok = false; // Soft ban
                proxy_pool_.report(*proxy_opt, ok);
            }

            // Success criteria for the PAGE (not the proxy)
            // 200 OK -> Good. 404 -> Good (server responded). 
            // 0 -> Bad (network). 403/429 -> Bad (blocked).
            bool page_ok = (res.success || res.status_code == 404) && res.status_code != 403 && res.status_code != 429;

                if (page_ok) {
                     if (res.status_code == 200) {
                        // Use effective_url for resolution and saving (handle redirects)
                        std::string base_url = !res.effective_url.empty() ? res.effective_url : current_url;

                        std::string markdown = Converter::to_markdown(res.body);
                        save_markdown(base_url, markdown);

                        if (depth < max_depth_) {
                            std::vector<std::string> links = Converter::extract_links(res.body);
                            int next_depth = depth + 1;
                            
                            for (const auto& link : links) {
                                std::string absolute_link = Url::resolve(base_url, link);
                                if (absolute_link.empty()) continue;
                                if (!Url::is_same_domain(start_domain_, absolute_link)) continue;
                                
                                add_url(std::move(absolute_link), next_depth);
                            }
                        }
                     } else {
                     Logger::warn("HTTP " + std::to_string(res.status_code) + ": " + current_url);
                 }
                 fetch_success = true;
                 break; // Done with this URL
            } else {
                // Retry needed
                if (attempt == 3) {
                    Logger::error("Failed: " + current_url + " (" + res.error + ") - Max retries reached");
                }
            }
        }
        
        if (fetch_success) {
             // Already handled save/links inside the loop
        } else {
             // All local retries failed.
             if (use_proxies_) {
                 if (!proxy_pool_.empty()) {
                     Logger::warn("Re-queueing URL (Proxy Rotation): " + current_url);
                     {
                         std::lock_guard<std::mutex> v_lock(visited_mutex_);
                         visited_.erase(current_url);
                     }
                     add_url(current_url, depth);
                 } else {
                     Logger::error("Giving up on URL (No active proxies left): " + current_url);
                 }
             } else {
                 Logger::error("Giving up on URL (Max retries): " + current_url);
             }
        }
        
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

}
