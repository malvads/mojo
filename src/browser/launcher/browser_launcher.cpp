#include "browser_launcher.hpp"
#include "logger/logger.hpp"
#include <iostream>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <cstdlib>
#include <thread>
#include <chrono>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
    #include <signal.h>
    #include <sys/wait.h>
#endif

namespace Mojo {
namespace Browser {
namespace Launcher {

using namespace Mojo::Core;

#ifdef _WIN32
    static PROCESS_INFORMATION browser_proc_info = {0};
#else
    static pid_t browser_pid = -1;
#endif

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
#elif defined(_WIN32)
    return {
        "C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe",
        "C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe",
        "C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe",
        "C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe"
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

bool BrowserLauncher::launch(const std::string& path, int port, bool headless, const std::string& proxy_url) {
#ifdef _WIN32
    if (browser_proc_info.hProcess != NULL) return true;
#else
    if (browser_pid != -1) return true;
#endif

    if (!std::filesystem::exists(path)) {
        Logger::error("Browser path does not exist: " + path);
        return false;
    }

    std::string port_arg = "--remote-debugging-port=" + std::to_string(port);
    std::string user_data_path;
    
#ifdef _WIN32
    user_data_path = std::filesystem::temp_directory_path().string() + "\\mojo_browser_" + std::to_string(GetCurrentProcessId());
#else
    user_data_path = "/tmp/mojo_browser_" + std::to_string(getpid());
#endif

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

    if (!proxy_url.empty()) {
        arg_strings.push_back("--proxy-server=" + proxy_url);
    }
    
    if (!headless) {
        auto it = std::find(arg_strings.begin(), arg_strings.end(), "--headless");
        if (it != arg_strings.end()) arg_strings.erase(it);
    }

#ifdef _WIN32
    std::string command_line;
    for (const auto& arg : arg_strings) {
        if (!command_line.empty()) command_line += " ";
        command_line += "\"" + arg + "\"";
    }

    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&browser_proc_info, sizeof(browser_proc_info));

    if (!CreateProcessA(NULL, const_cast<char*>(command_line.c_str()), NULL, NULL, FALSE, 0, NULL, NULL, &si, &browser_proc_info)) {
        Logger::error("Failed to launch browser: " + std::to_string(GetLastError()));
        return false;
    }
    Logger::info("Launched headless browser: " + path + " (PID: " + std::to_string(browser_proc_info.dwProcessId) + ")");
#else
    browser_pid = fork();
    if (browser_pid < 0) return false;

    if (browser_pid == 0) {
        std::vector<const char*> args;
        for (const auto& s : arg_strings) args.push_back(s.c_str());
        args.push_back(nullptr);

        if (freopen("/dev/null", "w", stdout) == NULL) {}
        if (freopen("/dev/null", "w", stderr) == NULL) {}

        execv(path.c_str(), const_cast<char* const*>(args.data()));
        exit(1);
    }
    Logger::info("Launched headless browser: " + path + " (PID: " + std::to_string(browser_pid) + ")");
#endif

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    return true;
}

void BrowserLauncher::cleanup() {
#ifdef _WIN32
    if (browser_proc_info.hProcess != NULL) {
        Logger::info("Closing headless browser (PID: " + std::to_string(browser_proc_info.dwProcessId) + ")...");
        TerminateProcess(browser_proc_info.hProcess, 0);
        CloseHandle(browser_proc_info.hProcess);
        CloseHandle(browser_proc_info.hThread);
        browser_proc_info.hProcess = NULL;
    }
#else
    if (browser_pid > 0) {
        Logger::info("Closing headless browser (PID: " + std::to_string(browser_pid) + ")...");
        kill(browser_pid, SIGTERM);
        waitpid(browser_pid, nullptr, 0);
        browser_pid = -1;
    }
#endif
}

}
}
}
