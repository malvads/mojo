#pragma once
#include <string>
#include <memory>
#include <vector>

namespace Mojo {

class BrowserLauncher {
public:
    struct BrowserInfo {
        std::string path;
        std::string name;
    };

    static std::string find_browser();
    static bool launch(const std::string& path, int port, bool headless, const std::string& proxy_url = "");
    static void cleanup();

private:
    static std::vector<std::string> get_search_paths();
};

}
