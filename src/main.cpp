#include <iostream>
#include <curl/curl.h>
#include "mojo/logger.hpp"
#include "mojo/crawler.hpp"
#include "mojo/config.hpp"
#include "mojo/browser_launcher.hpp"

namespace {



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
        crawler_config.proxy_priorities = config.proxy_priorities;
        crawler_config.proxy_retries = config.proxy_retries;
        crawler_config.browser_path = config.browser_path;
        crawler_config.headless = config.headless;
        crawler_config.proxy_threads = config.proxy_threads;
    
        crawler_config.proxy_bind_ip = config.proxy_bind_ip;
        crawler_config.proxy_bind_port = config.proxy_bind_port;
        crawler_config.cdp_port = config.cdp_port;

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

    run_crawler(config);

    return 0;
}
