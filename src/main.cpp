#include <curl/curl.h>
#include <iostream>
#include "crawler/crawler.hpp"
#include "config/config.hpp"
#include "launcher/browser_launcher.hpp"
#include "logger/logger.hpp"

namespace {

void run_crawler(const Mojo::Core::Config& config) {
    curl_global_init(CURL_GLOBAL_ALL);

    {
        Mojo::Engine::CrawlerConfig crawler_config;
        crawler_config.max_depth        = config.depth;
        crawler_config.threads          = config.threads;
        crawler_config.output_dir       = config.output_dir;
        crawler_config.tree_structure   = config.tree_structure;
        crawler_config.render_js        = config.render_js;
        crawler_config.proxies          = config.proxies;
        crawler_config.proxy_priorities = config.proxy_priorities;
        crawler_config.proxy_retries    = config.proxy_retries;
        crawler_config.browser_path     = config.browser_path;
        crawler_config.headless         = config.headless;
        crawler_config.proxy_threads    = config.proxy_threads;

        crawler_config.proxy_bind_ip   = config.proxy_bind_ip;
        crawler_config.proxy_bind_port = config.proxy_bind_port;
        crawler_config.cdp_port        = config.cdp_port;

        Mojo::Engine::Crawler crawler(crawler_config);

        for (const auto& url : config.urls) {
            crawler.start(url);
        }
    }

    curl_global_cleanup();
}

}  // namespace

int main(int argc, char* argv[]) {
    auto config = Mojo::Core::Config::parse(argc, argv);

    if (config.urls.empty()) {
        Mojo::Core::Logger::error("No URLs provided. Use --help for usage.");
        return 1;
    }

    run_crawler(config);

    return 0;
}
