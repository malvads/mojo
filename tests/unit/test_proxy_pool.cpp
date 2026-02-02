#include <atomic>
#include <gtest/gtest.h>
#include <thread>
#include <unordered_set>
#include "../../src/proxy/pool/proxy_pool.hpp"

using namespace Mojo::Proxy::Pool;

TEST(ProxyPoolTest, PriorityTiers) {
    std::vector<std::string> proxies = {
        "http://h1", "http://h2", "socks4://s4", "socks5://s5_1", "socks5://s5_2"};
    std::map<std::string, int> priorities = {{"http", 0}, {"socks4", 1}, {"socks5", 2}};
    ProxyPool                  pool(proxies, 3, priorities);

    // Should get socks5 first (priority 2)
    auto p1 = pool.get_proxy();
    EXPECT_EQ(p1->priority, ProxyPriority::SOCKS5);

    auto p2 = pool.get_proxy();
    EXPECT_EQ(p2->priority, ProxyPriority::SOCKS5);
    EXPECT_NE(p1->url, p2->url);
}

TEST(ProxyPoolTest, ExhaustionandRemoval) {
    std::vector<std::string>   proxies    = {"http://p1"};
    std::map<std::string, int> priorities = {{"http", 0}};
    ProxyPool                  pool(proxies, 0, priorities);  // 0 max retries, removes on 1st fail

    auto p1 = pool.get_proxy();
    ASSERT_TRUE(p1.has_value());
    pool.report(*p1, false);

    auto p2 = pool.get_proxy();
    EXPECT_FALSE(p2.has_value());
    EXPECT_TRUE(pool.empty());
}

TEST(ProxyPoolTest, SelectionFairness) {
    // With same priority, should alternate (round-robin logic in current impl)
    std::vector<std::string>   proxies    = {"http://a", "http://b", "http://c"};
    std::map<std::string, int> priorities = {{"http", 0}};
    ProxyPool                  pool(proxies, 3, priorities);

    std::unordered_set<std::string> order;
    for (int i = 0; i < 3; ++i) {
        auto p = pool.get_proxy();
        if (p.has_value())
            order.insert(p->url);
    }

    EXPECT_EQ(order.size(), 3);  // All unique proxies used
}

TEST(ProxyPoolTest, FailureTierShift) {
    std::vector<std::string>   proxies    = {"socks5://high", "http://low"};
    std::map<std::string, int> priorities = {{"socks5", 5}, {"http", 1}};
    ProxyPool                  pool(proxies, 1, priorities);

    // First get high
    auto p1 = pool.get_proxy();
    EXPECT_EQ(p1->url, "socks5://high");

    // Fail high twice -> removed
    pool.report(*p1, false);
    pool.report(*p1, false);

    // Now should get low
    auto p2 = pool.get_proxy();
    EXPECT_EQ(p2->url, "http://low");
}

TEST(ProxyPoolTest, StressMultiThreaded) {
    std::vector<std::string> proxies;
    for (int i = 0; i < 100; ++i)
        proxies.push_back("http://proxy_" + std::to_string(i));
    std::map<std::string, int> priorities = {{"http", 0}};
    ProxyPool                  pool(proxies, 10, priorities);

    std::vector<std::thread> threads;
    std::atomic<int>         success_count{0};
    for (int i = 0; i < 20; ++i) {
        threads.emplace_back([&pool, &success_count]() {
            for (int j = 0; j < 50; ++j) {
                auto p = pool.get_proxy();
                if (p.has_value()) {
                    pool.report(*p, (j % 5 != 0));  // Fail 20%
                    success_count++;
                }
            }
        });
    }
    for (auto& t : threads)
        t.join();
    EXPECT_GT(success_count, 0);
}

TEST(ProxyPoolTest, MultiThreadedFailureExhaustive) {
    std::vector<std::string>   proxies    = {"http://p1", "http://p2"};
    std::map<std::string, int> priorities = {{"http", 0}};
    ProxyPool                  pool(proxies, 5, priorities);

    std::vector<std::thread> threads;
    for (int i = 0; i < 20; ++i) {
        threads.emplace_back([&pool]() {
            for (int j = 0; j < 20; ++j) {
                auto p = pool.get_proxy();
                if (p)
                    pool.report(*p, false);
            }
        });
    }
    for (auto& t : threads)
        t.join();

    EXPECT_TRUE(pool.empty());
}

TEST(ProxyPoolTest, ExhaustionAndRecoverySim) {
    std::vector<std::string>   proxies    = {"http://p1"};
    std::map<std::string, int> priorities = {{"http", 0}};
    ProxyPool                  pool(proxies, 1, priorities);

    auto p = pool.get_proxy();
    if (p)
        pool.report(*p, false);
    if (p)
        pool.report(*p, false);  // Second fail should remove it

    EXPECT_FALSE(pool.get_proxy().has_value());
    EXPECT_TRUE(pool.empty());
}
