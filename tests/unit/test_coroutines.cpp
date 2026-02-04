#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_awaitable.hpp>
#ifndef CPPCHECK
#include <gtest/gtest.h>
#else
#define TEST(a, b) void a##_##b()
#define TEST_F(a, b) void a##_##b()
#define EXPECT_EQ(a, b)
#define EXPECT_TRUE(a)
#define EXPECT_FALSE(a)
namespace testing { class Test {}; }
#endif

boost::asio::awaitable<void> simple_coroutine(bool& executed) {
    executed = true;
    co_return;
}

// Basic execution test
TEST(CoroutineTest, BasicExecution) {
    boost::asio::io_context io_context;
    bool                    executed = false;

    boost::asio::co_spawn(io_context, simple_coroutine(executed), boost::asio::detached);

    io_context.run();

    EXPECT_TRUE(executed);
}

boost::asio::awaitable<int> async_add(int a, int b) {
    co_return a + b;
}

boost::asio::awaitable<void> task_coroutine(int& result) {
    result = co_await async_add(10, 20);
    co_return;
}

TEST(CoroutineTest, AssetResult) {
    boost::asio::io_context io_context;
    int                     result = 0;

    boost::asio::co_spawn(io_context, task_coroutine(result), boost::asio::detached);

    io_context.run();

    EXPECT_EQ(result, 30);
}
