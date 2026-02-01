#include "mojo/logger.hpp"
#include <iostream>

namespace Mojo {

int Logger::level_ = LogLevel::LOG_ALL;

// ... (skipping constants)

void Logger::info(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (level_ & LogLevel::LOG_INFO) {
        std::cout << BLUE << "[INFO] " << RESET << message << std::endl;
    }
}

void Logger::success(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (level_ & LogLevel::LOG_SUCCESS) {
        std::cout << GREEN << "[SUCCESS] " << RESET << message << std::endl;
    }
}

void Logger::warn(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (level_ & LogLevel::LOG_WARN) {
        std::cerr << YELLOW << "[WARN] " << RESET << message << std::endl;
    }
}

void Logger::error(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (level_ & LogLevel::LOG_ERROR) {
        std::cerr << RED << "[ERROR] " << RESET << message << std::endl;
    }
}

}
