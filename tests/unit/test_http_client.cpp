#include <boost/asio.hpp>
#include <chrono>
#include <gtest/gtest.h>
#include "../../src/network/http/beast_client.hpp"

using namespace Mojo::Network::Http;

class HttpClientTest : public ::testing::Test {
protected:
    boost::asio::io_context ioc;
};

TEST_F(HttpClientTest, StateManagement) {
    BeastClient client(ioc);
    client.set_connect_timeout(std::chrono::milliseconds(2000));
    client.set_proxy("socks5://localhost:1080");
}

TEST_F(HttpClientTest, ProxyParsing) {
    BeastClient client(ioc);
    // Verified setting proxy doesn't crash
    client.set_proxy("http://proxy.example.com:8080");
    client.set_proxy("");
}
