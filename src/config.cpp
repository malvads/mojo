#include <iostream>
#include <fstream>
#include <yaml-cpp/yaml.h>
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
            if (i + 1 < argc) {
                config.config_path = argv[++i];
            }
        } else if (arg == "--proxy-bind-ip") {
            if (i + 1 < argc) config.proxy_bind_ip = argv[++i];
        } else if (arg == "--proxy-bind-port") {
            if (i + 1 < argc) config.proxy_bind_port = std::stoi(argv[++i]);
        } else if (arg == "--cdp-port") {
            if (i + 1 < argc) config.cdp_port = std::stoi(argv[++i]);
        } else {
            config.urls.push_back(arg);
        }
    }

    if (!config.config_path.empty()) {
        try {
            YAML::Node yaml_config = YAML::LoadFile(config.config_path);
            if (yaml_config["depth"]) config.depth = yaml_config["depth"].as<int>();
            if (yaml_config["max_depth"]) config.depth = yaml_config["max_depth"].as<int>();
            if (yaml_config["threads"]) config.threads = yaml_config["threads"].as<int>();
            if (yaml_config["output"]) config.output_dir = yaml_config["output"].as<std::string>();
            if (yaml_config["output_dir"]) config.output_dir = yaml_config["output_dir"].as<std::string>();
            if (yaml_config["render"]) config.render_js = yaml_config["render"].as<bool>();
            if (yaml_config["render_js"]) config.render_js = yaml_config["render_js"].as<bool>();
            if (yaml_config["headless"]) config.headless = yaml_config["headless"].as<bool>();
            if (yaml_config["browser_path"]) config.browser_path = yaml_config["browser_path"].as<std::string>();
            if (yaml_config["proxy_retries"]) config.proxy_retries = yaml_config["proxy_retries"].as<int>();
            
            if (yaml_config["proxy_bind_ip"]) config.proxy_bind_ip = yaml_config["proxy_bind_ip"].as<std::string>();
            if (yaml_config["proxy_bind_port"]) config.proxy_bind_port = yaml_config["proxy_bind_port"].as<int>();
            if (yaml_config["cdp_port"]) config.cdp_port = yaml_config["cdp_port"].as<int>();

            if (yaml_config["proxies"]) {
                auto proxies_node = yaml_config["proxies"];
                if (proxies_node.IsSequence()) {
                    for (const auto& node : proxies_node) {
                        config.proxies.push_back(node.as<std::string>());
                    }
                }
            }

            if (yaml_config["proxy_list"]) {
                std::ifstream file(yaml_config["proxy_list"].as<std::string>());
                std::string line;
                while (std::getline(file, line)) {
                    if (!line.empty()) {
                        config.proxies.push_back(line);
                    }
                }
            }

            YAML::Node priorities_node;
            if (yaml_config["proxy_priorities"]) priorities_node = yaml_config["proxy_priorities"];
            else if (yaml_config["priorities"]) priorities_node = yaml_config["priorities"];

            if (priorities_node && priorities_node.IsMap()) {
                for (YAML::const_iterator it = priorities_node.begin(); it != priorities_node.end(); ++it) {
                    std::string protocol = it->first.as<std::string>();
                    int priority = it->second.as<int>();
                    config.proxy_priorities[protocol] = priority;
                }
            }
        } catch (const YAML::Exception& e) {
            std::cerr << "Error parsing config file: " << e.what() << std::endl;
            exit(1);
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
              << "  --proxy-bind-ip <ip>  Proxy Server bind IP (default: 127.0.0.1)\n"
              << "  --proxy-bind-port <n> Proxy Server bind Port (default: 0 [random])\n"
              << "  --cdp-port <n>        Chrome DevTools Protocol port (default: 9222)\n"
              << "  -o, --output <dir>    Output directory (default: mojo_out)\n"
              << "  --flat                Use flat structure (default: tree)\n"
              << "  --render              Enable JavaScript rendering (Experimental)\n"
              << "  --browser <path>      Path to Chromium/Chrome executable\n"
              << "  --config <file>       Path to YAML configuration file\n"
              << "  --no-headless         Run browser in windowed mode (debug only)\n"
              << "  -h, --help            Show this help message\n";
}

}
