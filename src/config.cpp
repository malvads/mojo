#include <iostream>
#include <fstream>
#include "mojo/config.hpp"

namespace Mojo {

Config Config::parse(int argc, char* argv[]) {
    Config config;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            config.show_help = true;
            return config;
        } else if (arg == "-d" || arg == "--depth") {
            if (i + 1 < argc) {
                config.depth = std::stoi(argv[++i]);
            }
        } else if (arg == "-t" || arg == "--threads") {
            if (i + 1 < argc) {
                config.threads = std::stoi(argv[++i]);
            }
            if (i + 1 < argc) {
                config.proxies.push_back(argv[++i]);
            }
        } else if (arg == "--proxy-retries") {
            if (i + 1 < argc) {
                config.proxy_retries = std::stoi(argv[++i]);
            }
        } else if (arg == "--proxy-list") {
            if (i + 1 < argc) {
                std::ifstream file(argv[++i]);
                std::string line;
                while (std::getline(file, line)) {
                    if (!line.empty()) {
                        config.proxies.push_back(line);
                    }
                }
            }
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 < argc) {
                config.output_dir = argv[++i];
            }
        } else if (arg == "--flat") {
            config.tree_structure = false;
        } else if (arg == "--render") {
            config.render_js = true;
        } else if (arg == "--browser") {
            if (i + 1 < argc) {
                config.browser_path = argv[++i];
            }
        } else if (arg == "--no-headless") {
            config.headless = false;
        } else {
            config.urls.push_back(arg);
        }
    }
    return config;
}

void Config::print_usage(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " [options] <url1> [url2] ...\n"
              << "Options:\n"
              << "  -d, --depth <n>       Crawling depth (default: 0)\n"
              << "  -t, --threads <n>     Number of threads (default: 4)\n"
              << "  -p, --proxy <url>     Single proxy (e.g., socks5://127.0.0.1:9050)\n"
              << "  --proxy-list <file>   File containing list of proxies\n"
              << "  --proxy-retries <n>   Max retries before removing a proxy (default: 3)\n"
              << "  -o, --output <dir>    Output directory (default: mojo_out)\n"
              << "  --flat                Use flat structure (default: tree)\n"
              << "  --render              Enable JavaScript rendering (Experimental)\n"
              << "  --browser <path>      Path to Chromium/Chrome executable\n"
              << "  --no-headless         Run browser in windowed mode (debug only)\n"
              << "  -h, --help            Show this help message\n";
}

}
