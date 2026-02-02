#include "config/config.hpp"
#include <CLI/CLI.hpp>
#include <fstream>
#include <iostream>
#include <yaml-cpp/yaml.h>

namespace Mojo {
namespace Core {

void load_yaml(Config& config, const std::string& path) {
    try {
        YAML::Node yaml = YAML::LoadFile(path);
        if (yaml["depth"])
            config.depth = yaml["depth"].as<int>();
        if (yaml["max_depth"])
            config.depth = yaml["max_depth"].as<int>();
        if (yaml["threads"])
            config.threads = yaml["threads"].as<int>();
        if (yaml["output"])
            config.output_dir = yaml["output"].as<std::string>();
        if (yaml["output_dir"])
            config.output_dir = yaml["output_dir"].as<std::string>();
        if (yaml["render"])
            config.render_js = yaml["render"].as<bool>();
        if (yaml["render_js"])
            config.render_js = yaml["render_js"].as<bool>();
        if (yaml["headless"])
            config.headless = yaml["headless"].as<bool>();
        if (yaml["browser_path"])
            config.browser_path = yaml["browser_path"].as<std::string>();
        if (yaml["proxy_retries"])
            config.proxy_retries = yaml["proxy_retries"].as<int>();
        if (yaml["proxy_bind_ip"])
            config.proxy_bind_ip = yaml["proxy_bind_ip"].as<std::string>();
        if (yaml["proxy_bind_port"])
            config.proxy_bind_port = yaml["proxy_bind_port"].as<int>();
        if (yaml["cdp_port"])
            config.cdp_port = yaml["cdp_port"].as<int>();
        if (yaml["proxy_threads"])
            config.proxy_threads = yaml["proxy_threads"].as<int>();

        if (yaml["proxies"] && yaml["proxies"].IsSequence()) {
            for (const auto& node : yaml["proxies"])
                config.proxies.push_back(node.as<std::string>());
        }

        if (yaml["proxy_list"]) {
            std::ifstream file(yaml["proxy_list"].as<std::string>());
            std::string   line;
            while (std::getline(file, line))
                if (!line.empty())
                    config.proxies.push_back(line);
        }

        YAML::Node prio = yaml["proxy_priorities"] ? yaml["proxy_priorities"] : yaml["priorities"];
        if (prio && prio.IsMap()) {
            for (auto it = prio.begin(); it != prio.end(); ++it) {
                config.proxy_priorities[it->first.as<std::string>()] = it->second.as<int>();
            }
        }
    } catch (const YAML::Exception& e) {
        throw std::runtime_error("Error parsing config file: " + std::string(e.what()));
    }
}

Config Config::parse(int argc, char* argv[]) {
    Config   config;
    CLI::App app{"Mojo - Extremely Fast Web Crawler for AI & LLM Data Ingestion"};

    std::string proxy_list_path;
    std::string single_proxy;

    app.add_option("-d,--depth", config.depth, "Crawling depth");
    app.add_option("-t,--threads", config.threads, "Number of threads");
    app.add_option("-o,--output", config.output_dir, "Output directory");
    app.add_option("-p,--proxy", single_proxy, "Single proxy URL");
    app.add_option("--proxy-list", proxy_list_path, "File containing list of proxies");
    app.add_option("--proxy-retries", config.proxy_retries, "Max failures before removing a proxy");
    app.add_option("--proxy-threads", config.proxy_threads, "Threads for proxy gateway");
    app.add_option("--proxy-bind-ip", config.proxy_bind_ip, "Proxy Server bind IP");
    app.add_option("--proxy-bind-port", config.proxy_bind_port, "Proxy Server bind Port");
    app.add_option("--cdp-port", config.cdp_port, "Chrome DevTools Protocol port");
    app.add_option("--browser", config.browser_path, "Path to Chromium/Chrome executable");
    app.add_option("--config", config.config_path, "Path to YAML configuration file");

    app.add_flag(
        "--flat",
        [&](size_t count) {
            if (count > 0)
                config.tree_structure = false;
        },
        "Use flat output structure");
    app.add_flag("--render", config.render_js, "Enable JavaScript rendering");
    app.add_flag(
        "--no-headless",
        [&](size_t count) {
            if (count > 0)
                config.headless = false;
        },
        "Run browser in windowed mode (debug only)");

    app.add_option("urls", config.urls, "URLs to crawl");

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        exit(app.exit(e));
    }

    if (!config.config_path.empty()) {
        load_yaml(config, config.config_path);

        try {
            app.parse(argc, argv);
        } catch (const CLI::ParseError& e) {
            exit(app.exit(e));
        }
    }

    if (!single_proxy.empty())
        config.proxies.push_back(single_proxy);
    if (!proxy_list_path.empty()) {
        std::ifstream file(proxy_list_path);
        std::string   line;
        while (std::getline(file, line)) {
            // Trim whitespace
            size_t f = line.find_first_not_of(" \t\r\n");
            if (f == std::string::npos)
                continue;
            line = line.substr(f);

            // Strip comments
            size_t hash = line.find('#');
            if (hash != std::string::npos) {
                line = line.substr(0, hash);
                // re-trim
                size_t last = line.find_last_not_of(" \t\r\n");
                if (last == std::string::npos)
                    continue;
                line = line.substr(0, last + 1);
            }

            if (!line.empty())
                config.proxies.push_back(line);
        }
    }

    return config;
}

}  // namespace Core
}  // namespace Mojo
