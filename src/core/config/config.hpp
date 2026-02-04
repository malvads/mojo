#pragma once
#include <string>
#include <vector>

#include "../types/constants.hpp"

namespace Mojo {
namespace Core {

struct Config {
    int                        depth           = Constants::DEFAULT_DEPTH;
    int                        threads         = Constants::DEFAULT_THREADS;
    int                        virtual_threads = Constants::DEFAULT_VIRTUAL_THREADS;
    int                        worker_threads  = Constants::DEFAULT_WORKER_THREADS;
    std::vector<std::string>   proxies;
    std::vector<std::string>   urls;
    std::string                output_dir = Constants::DEFAULT_OUTPUT_DIR;
    std::string                config_path;
    std::map<std::string, int> proxy_priorities = {{"http", 0}, {"socks4", 1}, {"socks5", 2}};
    int                        proxy_retries    = Constants::DEFAULT_PROXY_RETRIES;
    bool                       tree_structure   = true;
    bool                       render_js        = false;
    std::string                browser_path;
    bool                       headless              = true;
    int                        proxy_connect_timeout = 5000;  // milliseconds

    std::string proxy_bind_ip   = "127.0.0.1";
    int         proxy_bind_port = 0;  // 0 = Random
    int         cdp_port        = 9222;

    int proxy_threads = 32;

    static Config parse(int argc, char* argv[]);
};

}  // namespace Core
}  // namespace Mojo
