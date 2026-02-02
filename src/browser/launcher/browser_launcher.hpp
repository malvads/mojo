#pragma once
#include <memory>
#include <string>
#include <vector>

namespace Mojo {
namespace Browser {
namespace Launcher {

class BrowserLauncher {
public:
    struct BrowserInfo {
        std::string path;
        std::string name;
    };

    static std::string find_browser();
    static bool
    launch(const std::string& path, int port, bool headless, const std::string& proxy_url = "");
    static void cleanup();

private:
    static std::vector<std::string> get_search_paths();
};

}  // namespace Launcher
}  // namespace Browser
}  // namespace Mojo
