#include "logger/logger.hpp"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace Mojo {
namespace Core {

int Logger::level_ = LogLevel::LOG_ALL;

std::mutex Logger::mutex_;

namespace {

// ANSI color codes for terminal output formatting
const std::string RESET  = "\033[0m";
const std::string RED    = "\033[31m";
const std::string GREEN  = "\033[32m";
const std::string YELLOW = "\033[33m";
const std::string BLUE   = "\033[34m";

/**
 * @brief Gets the current timestamp as a formatted string.
 *
 * Returns the current time in ISO 8601 format (YYYY-MM-DD HH:MM:SS)
 * for use in log message prefixes.
 *
 * @return std::string The formatted timestamp string.
 */
std::string get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

}  // namespace

void Logger::set_level(int level) {
    std::lock_guard<std::mutex> lock(mutex_);
    level_ = level;
}

void Logger::info(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (level_ & LogLevel::LOG_INFO) {
        std::cout << BLUE << "[INFO] " << RESET << "[" << get_timestamp() << "] " << message << std::endl;
    }
}

void Logger::success(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (level_ & LogLevel::LOG_SUCCESS) {
        std::cout << GREEN << "[SUCCESS] " << RESET << "[" << get_timestamp() << "] " << message << std::endl;
    }
}

void Logger::warn(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (level_ & LogLevel::LOG_WARN) {
        std::cerr << YELLOW << "[WARN] " << RESET << "[" << get_timestamp() << "] " << message << std::endl;
    }
}

void Logger::error(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (level_ & LogLevel::LOG_ERROR) {
        std::cerr << RED << "[ERROR] " << RESET << "[" << get_timestamp() << "] " << message << std::endl;
    }
}

}  // namespace Core
}  // namespace Mojo
