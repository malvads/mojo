#include "mojo/browser_launcher.hpp"
#include "mojo/logger.hpp"
#include <iostream>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

namespace Mojo {

static pid_t browser_pid = -1;

std::vector<std::string> BrowserLauncher::get_search_paths() {
#ifdef __APPLE__
    return {
        "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome",
        "/Applications/Chromium.app/Contents/MacOS/Chromium",
        "/opt/homebrew/bin/chromium",
        "/usr/local/bin/chromium",
        "/Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge",
        "/usr/local/bin/chrome",
        "/usr/bin/google-chrome"
    };
#else
    return {
        "/usr/bin/google-chrome",
        "/usr/bin/chromium-browser",
        "/usr/bin/chromium",
        "google-chrome",
        "chromium-browser",
        "chromium"
    };
#endif
}

std::string BrowserLauncher::find_browser() {
    for (const auto& path : get_search_paths()) {
        if (std::filesystem::exists(path)) return path;
    }
    return "";
}

bool BrowserLauncher::launch(const std::string& path, int port, bool headless) {
    if (browser_pid != -1) return true;

    browser_pid = fork();
    if (browser_pid < 0) return false;

    if (browser_pid == 0) {
        std::string port_arg = "--remote-debugging-port=" + std::to_string(port);
        std::string user_data_path = "/tmp/mojo_browser_" + std::to_string(getpid());
        
        std::filesystem::create_directories(user_data_path);
        std::string user_data = "--user-data-dir=" + user_data_path;
        
        std::vector<std::string> arg_strings = {
            path,
            "--headless",
            "--disable-gpu",
            "--disable-extensions",
            "--disable-backgrounding-occluded-windows",
            "--disable-renderer-backgrounding",
            "--window-size=1920,1080",
            "--hide-scrollbars",
            "--disable-notifications",
            "--no-sandbox",
            port_arg,
            user_data,
            "--remote-allow-origins=*"
        };
        
        if (!headless) {
            auto it = std::find(arg_strings.begin(), arg_strings.end(), "--headless");
            if (it != arg_strings.end()) arg_strings.erase(it);
        }

        std::vector<const char*> args;
        for (const auto& s : arg_strings) args.push_back(s.c_str());
        args.push_back(nullptr);

        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);

        execv(path.c_str(), const_cast<char* const*>(args.data()));
        exit(1);
    }

    Logger::info("Launched headless browser: " + path + " (PID: " + std::to_string(browser_pid) + ")");
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    return true;
}

void BrowserLauncher::cleanup() {
    if (browser_pid > 0) {
        Logger::info("Closing headless browser (PID: " + std::to_string(browser_pid) + ")...");
        kill(browser_pid, SIGTERM);
        waitpid(browser_pid, nullptr, 0);
        browser_pid = -1;
    }
}

}
