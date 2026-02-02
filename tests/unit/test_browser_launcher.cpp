#include <gtest/gtest.h>
#include "../../src/browser/launcher/browser_launcher.hpp"

using namespace Mojo::Browser::Launcher;

TEST(LauncherTest, FindBrowser) {
    std::string path = BrowserLauncher::find_browser();
#ifdef __APPLE__
    if (!path.empty()) {
        EXPECT_TRUE(path.find("Contents/MacOS") != std::string::npos || path.find("/usr/local/bin") != std::string::npos);
    }
#endif
}

TEST(LauncherTest, LaunchSmoke) {
    bool success = BrowserLauncher::launch("/non/existent/path", 9999, true);
    EXPECT_FALSE(success);
}
