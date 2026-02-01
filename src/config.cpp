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
        } else if (arg == "-p" || arg == "--proxy") {
            if (i + 1 < argc) {
                config.proxies.push_back(argv[++i]);
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
        } else {
            config.urls.push_back(arg);
        }
    }
    return config;
}

}
