#pragma once
#include <string>
#include <mutex>

namespace Mojo {

enum LogLevel {
    LOG_NONE    = 0,
    LOG_INFO    = 1 << 0,
    LOG_WARN    = 1 << 1,
    LOG_ERROR   = 1 << 2,
    LOG_SUCCESS = 1 << 3,
    LOG_ALL     = LOG_INFO | LOG_WARN | LOG_ERROR | LOG_SUCCESS
};

class Logger {
public:
    static void set_level(int level);
    static void info(const std::string& message);
    static void success(const std::string& message);
    static void warn(const std::string& message);
    static void error(const std::string& message);

private:
    static int level_;
    static std::mutex mutex_;
};

}
