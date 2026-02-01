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
    bool tree_structure = true;
    bool show_help = false;

    static Config parse(int argc, char* argv[]);
};

}
