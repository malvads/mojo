#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include "../../src/core/logger/logger.hpp"

using namespace Mojo::Core;

TEST(LoggerTest, SetLevel) {
    Logger::set_level(LOG_NONE);
    Logger::info("Test info message - hidden");
}

TEST(LoggerTest, LevelFiltering) {
    Logger::set_level(LOG_ERROR);
    Logger::info("This should not be printed");
    Logger::error("This should be printed");
    Logger::set_level(LOG_ALL);
}

TEST(LoggerTest, StressTest) {
    Logger::set_level(LOG_ALL);
    std::vector<std::thread> threads;
    for(int i = 0; i < 50; ++i) {
        threads.emplace_back([]() {
            for(int j = 0; j < 100; ++j) {
                Logger::info("Logging from thread " + std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())));
            }
        });
    }
    for(auto& t : threads) t.join();
}

TEST(LoggerTest, LargeMessage) {
    std::string large(1024 * 1024, 'A');
    Logger::info("Large message test: " + large.substr(0, 10) + "... [size: " + std::to_string(large.size()) + "]");
}
