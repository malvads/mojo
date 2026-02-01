#pragma once
#include <vector>
#include <string>

namespace Mojo {

struct Config {
    int depth = 0;
    int threads = 4;
    std::vector<std::string> proxies;
    std::vector<std::string> urls;
    std::string output_dir = "mojo_out";
    bool tree_structure = true;
    bool show_help = false;

    static Config parse(int argc, char* argv[]);
};

}
