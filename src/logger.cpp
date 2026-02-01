#include "mojo/logger.hpp"
#include <iostream>

namespace Mojo {

int Logger::level_ = LogLevel::ALL;
std::mutex Logger::mutex_;

namespace {
    const std::string RESET   = "\033[0m";
    const std::string RED     = "\033[31m";
    const std::string GREEN   = "\033[32m";
    const std::string YELLOW  = "\033[33m";
    const std::string BLUE    = "\033[34m";
}

void Logger::set_level(int level) {
    std::lock_guard<std::mutex> lock(mutex_);
    level_ = level;
}

void Logger::info(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (level_ & LogLevel::INFO) {
        std::cout << BLUE << "[INFO] " << RESET << message << std::endl;
    }
}

void Logger::success(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (level_ & LogLevel::SUCCESS) {
        std::cout << GREEN << "[SUCCESS] " << RESET << message << std::endl;
    }
}

void Logger::warn(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (level_ & LogLevel::WARN) {
        std::cerr << YELLOW << "[WARN] " << RESET << message << std::endl;
    }
}

void Logger::error(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (level_ & LogLevel::ERROR) {
        std::cerr << RED << "[ERROR] " << RESET << message << std::endl;
    }
}

}
