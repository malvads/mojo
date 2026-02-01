#include <iostream>
#include <curl/curl.h>

#include "mojo/logger.hpp"
#include "mojo/crawler.hpp"
#include "mojo/config.hpp"

void print_usage(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " [options] <url1> [url2] ...\n"
              << "Options:\n"
              << "  -d, --depth <n>       Crawling depth (default: 0)\n"
              << "  -t, --threads <n>     Number of threads (default: 4)\n"
              << "  -p, --proxy <url>     Single proxy (e.g., socks5://127.0.0.1:9050)\n"
              << "  --proxy-list <file>   File containing list of proxies\n"
              << "  -o, --output <dir>    Output directory (default: mojo_out)\n"
              << "  --flat                Use flat structure (default: tree)\n"
              << "  -h, --help            Show this help message\n";
}

int main(int argc, char* argv[]) {
    Mojo::Config config = Mojo::Config::parse(argc, argv);
    
    if (config.show_help) {
        print_usage(argv[0]);
        return 0;
    }
    
    if (config.urls.empty()) {
        Mojo::Logger::error("No URLs provided.");
        print_usage(argv[0]);
        return 1;
    }

    curl_global_init(CURL_GLOBAL_ALL);

    Mojo::Crawler crawler(config.depth, config.threads, 
                          config.output_dir, config.tree_structure, 
                          config.proxies);

    for (const auto& url : config.urls) {
        crawler.start(url);
    }

    curl_global_cleanup();
    return 0;
}
