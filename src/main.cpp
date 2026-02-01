#include <iostream>
#include <curl/curl.h>
#include "mojo/logger.hpp"
#include "mojo/crawler.hpp"
#include "mojo/config.hpp"
#include "mojo/browser_launcher.hpp"

namespace {

void launch_browser_if_needed(const Mojo::Config& config) {
    if (!config.render_js) return;

    std::string path = config.browser_path;
    if (path.empty()) path = Mojo::BrowserLauncher::find_browser();
    
    if (path.empty()) {
        Mojo::Logger::error("No Chromium browser found. Use --browser to specify path.");
        exit(1);
    }

    if (!Mojo::BrowserLauncher::launch(path, 9222, config.headless)) {
        Mojo::Logger::error("Failed to launch headless browser.");
        exit(1);
    }

    std::atexit(Mojo::BrowserLauncher::cleanup);
}

void run_crawler(const Mojo::Config& config) {
    curl_global_init(CURL_GLOBAL_ALL);

    {
        Mojo::CrawlerConfig crawler_config;
        crawler_config.max_depth = config.depth;
        crawler_config.threads = config.threads;
        crawler_config.output_dir = config.output_dir;
        crawler_config.tree_structure = config.tree_structure;
        crawler_config.render_js = config.render_js;
        crawler_config.proxies = config.proxies;
        crawler_config.proxy_retries = config.proxy_retries;

        Mojo::Crawler crawler(crawler_config);

        for (const auto& url : config.urls) {
            crawler.start(url);
        }
    }

    curl_global_cleanup();
}

} // namespace

int main(int argc, char* argv[]) {
    auto config = Mojo::Config::parse(argc, argv);
    
    if (config.show_help) {
        Mojo::Config::print_usage(argv[0]);
        return 0;
    }
    
    if (config.urls.empty()) {
        Mojo::Logger::error("No URLs provided.");
        Mojo::Config::print_usage(argv[0]);
        return 1;
    }

    launch_browser_if_needed(config);
    run_crawler(config);

    return 0;
}
