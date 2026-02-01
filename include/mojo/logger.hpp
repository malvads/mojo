#pragma once
#include <string>
#include <mutex>

namespace Mojo {

enum LogLevel {
    NONE    = 0,
    INFO    = 1 << 0,
    WARN    = 1 << 1,
    ERROR   = 1 << 2,
    SUCCESS = 1 << 3,
    ALL     = INFO | WARN | ERROR | SUCCESS
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
