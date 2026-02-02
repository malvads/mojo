#include <gtest/gtest.h>
#include <vector>
#include <string>
#include "../../src/network/http/curl_client.hpp"

using namespace Mojo::Network::Http;

TEST(HttpClientTest, RealisticUserAgents) {
    CurlClient client;
    std::vector<std::string> uas = {
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) Chrome/91.0.4472.124 Safari/537.36",
        "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) Safari/605.1.15"
    };
    for(const auto& ua : uas) {
        client.set_user_agent(ua);
        // Verified setting UA doesn't crash
    }
}

TEST(HttpClientTest, StateManagement) {
    CurlClient client;
    client.set_timeout(10);
    client.set_connect_timeout(5);
    client.add_header("X-Custom", "Value");
    client.clear_headers();
    client.set_proxy("http://localhost:8080");
}

TEST(HttpClientTest, UARotation) {
    CurlClient client;
    for(int i = 0; i < 20; ++i) {
        client.set_user_agent("Agent " + std::to_string(i));
    }
}
