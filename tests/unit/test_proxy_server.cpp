#include <chrono>
#include <gtest/gtest.h>
#include <httplib.h>
#include <thread>
#include "../../src/proxy/pool/proxy_pool.hpp"
#include "../../src/proxy/server/proxy_server.hpp"

using namespace Mojo::Proxy::Server;
using namespace Mojo::Proxy::Pool;

class ProxyServerTest : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

TEST_F(ProxyServerTest, BasicHttpProxy) {
    // 1. Start Upstream Server (Acting as the "Next Hop" Proxy or Destination)
    httplib::Server upstream;
    upstream.Get("/target", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("Target Reached", "text/plain");
    });

    int         upstream_port = 9095;
    std::thread upstream_thread([&]() { upstream.listen("127.0.0.1", upstream_port); });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 2. Setup ProxyPool pointing to Upstream
    // The ProxyServer will forward traffic to this "proxy" (which is actually our destination
    // server)
    std::vector<std::string>   proxy_list = {"http://127.0.0.1:" + std::to_string(upstream_port)};
    std::map<std::string, int> priorities;
    ProxyPool                  pool(proxy_list, 1, priorities);

    // 3. Start ProxyServer under test
    // Listens on 9096
    ProxyServer proxy_server(pool, "127.0.0.1", 9096, 1);
    proxy_server.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 4. Client connects to ProxyServer (9096)
    // We send a request that looks like "GET /target" with "Host: 127.0.0.1:9095"
    // The ProxyServer will:
    //  a. Parse Host: 127.0.0.1:9095
    //  b. Ask ProxyPool for a proxy. Pool returns 127.0.0.1:9095 (the only one).
    //  c. Connect to 127.0.0.1:9095.
    //  d. Forward the "GET /target" request to 127.0.0.1:9095.
    //  e. 127.0.0.1:9095 (Upstream) handles /target and replies.

    httplib::Client  client("127.0.0.1", 9096);
    httplib::Headers headers = {{"Host", "127.0.0.1:" + std::to_string(upstream_port)}};

    auto res = client.Get("/target", headers);

    if (res) {
        EXPECT_EQ(res->status, 200);
        EXPECT_EQ(res->body, "Target Reached");
    }
    else {
        FAIL() << "Client failed to connect to ProxyServer";
    }

    proxy_server.stop();
    upstream.stop();
    upstream_thread.join();
}
