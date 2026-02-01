#pragma once
#include <vector>
#include <string>

#include "mojo/constants.hpp"

namespace Mojo {

struct Config {
    int depth = Constants::DEFAULT_DEPTH;
    int threads = Constants::DEFAULT_THREADS;
    std::vector<std::string> proxies;
    std::vector<std::string> urls;
    std::string output_dir = Constants::DEFAULT_OUTPUT_DIR;
    std::string config_path;
    std::map<std::string, int> proxy_priorities = {
        {"http", 0},
        {"socks4", 1},
        {"socks5", 2}
    };
    int proxy_retries = Constants::DEFAULT_PROXY_RETRIES;
    bool tree_structure = true;
    bool render_js = false;
    std::string browser_path;
    bool headless = true;
    bool show_help = false;

    static Config parse(int argc, char* argv[]);
    static void print_usage(const char* prog_name);
};

}
