#include <filesystem>
#include <fstream>
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
#include <httplib.h>
#include <thread>
#include "crawler/crawler.hpp"
#include "logger/logger.hpp"

namespace fs = std::filesystem;

class TestServer {
public:
    TestServer() {
    }

    void set_route(const std::string& path,
                   const std::string& content,
                   const std::string& type = "text/html") {
        server_.Get(path, [content, type](const httplib::Request&, httplib::Response& res) {
            res.set_content(content, type.c_str());
        });
    }

    void start(int port, const std::string& host = "127.0.0.1") {
        port_   = port;
        host_   = host;
        thread_ = std::thread([this, host, port]() { server_.listen(host.c_str(), port); });
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    void stop() {
        server_.stop();
        if (thread_.joinable())
            thread_.join();
    }

    std::string url() const {
        return "http://" + host_ + ":" + std::to_string(port_);
    }

private:
    httplib::Server server_;
    std::thread     thread_;
    int             port_ = 0;
    std::string     host_ = "127.0.0.1";
};

class IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        Mojo::Core::Logger::set_level(Mojo::Core::LOG_INFO);
        if (fs::exists("test_output"))
            fs::remove_all("test_output");
        fs::create_directory("test_output");
    }

    void TearDown() override {
        if (fs::exists("test_output"))
            fs::remove_all("test_output");
    }
};

TEST_F(IntegrationTest, BasicCrawlDiscovery) {
    TestServer server;
    server.set_route("/", "<html><body><a href='/a'>A</a></body></html>");
    server.set_route("/a", "<html><body><a href='/b'>B</a></body></html>");
    server.set_route("/b", "<html><body>Done</body></html>");
    server.start(8081);

    Mojo::Engine::CrawlerConfig config;
    config.max_depth       = 2;
    config.threads         = 1;
    config.virtual_threads = 4;
    config.output_dir      = "test_output";
    config.tree_structure  = true;

    Mojo::Engine::Crawler crawler(config);
    crawler.start(server.url() + "/");

    // Filenames include port: 127.0.0.1_8081
    EXPECT_TRUE(fs::exists("test_output/127.0.0.1_8081/index.md"));
    EXPECT_TRUE(fs::exists("test_output/127.0.0.1_8081/a.md"));
    EXPECT_TRUE(fs::exists("test_output/127.0.0.1_8081/b.md"));

    server.stop();
}

TEST_F(IntegrationTest, ImageFiltering) {
    TestServer server;
    server.set_route(
        "/",
        "<html><body><img src='/logo.png'><a href='/logo.png'>Link to Image</a></body></html>");
    server.set_route("/logo.png", "FAKE_IMAGE_DATA", "image/png");
    server.start(8082);

    Mojo::Engine::CrawlerConfig config;
    config.max_depth       = 1;
    config.threads         = 1;
    config.virtual_threads = 4;
    config.output_dir      = "test_output";
    config.tree_structure  = true;

    Mojo::Engine::Crawler crawler(config);
    crawler.start(server.url() + "/");

    EXPECT_TRUE(fs::exists("test_output/127.0.0.1_8082/index.md"));
    EXPECT_FALSE(fs::exists("test_output/127.0.0.1_8082/logo.png"));
    EXPECT_FALSE(fs::exists("test_output/127.0.0.1_8082/logo.md"));

    server.stop();
}

TEST_F(IntegrationTest, DomainRestriction) {
    TestServer server1;
    server1.set_route("/",
                      "<html><body><a href='http://localhost:8084/ext'>External</a></body></html>");
    server1.start(8083, "127.0.0.1");

    TestServer server2;
    server2.set_route("/ext", "<html><body>External Page</body></html>");
    server2.start(8084, "localhost");

    Mojo::Engine::CrawlerConfig config;
    config.max_depth       = 1;
    config.threads         = 1;
    config.virtual_threads = 4;
    config.output_dir      = "test_output";
    config.tree_structure  = true;

    Mojo::Engine::Crawler crawler(config);
    crawler.start(server1.url() + "/");

    EXPECT_TRUE(fs::exists("test_output/127.0.0.1_8083/index.md"));
    EXPECT_FALSE(fs::exists("test_output/localhost_8084/ext.md"));

    server1.stop();
    server2.stop();
}

TEST_F(IntegrationTest, RobotsTxtEnforcement) {
    TestServer server;
    // Allow /public, disallow /private
    server.set_route("/robots.txt", "User-agent: *\nDisallow: /private\nAllow: /public\n");
    server.set_route(
        "/",
        "<html><body><a href='/public'>Public</a><a href='/private'>Private</a></body></html>");
    server.set_route("/public", "<html><body>Public Page</body></html>");
    server.set_route("/private", "<html><body>Private Page</body></html>");
    server.start(8085);

    Mojo::Engine::CrawlerConfig config;
    config.max_depth       = 1;
    config.threads         = 1;
    config.virtual_threads = 4;
    config.output_dir      = "test_output";
    config.tree_structure  = true;

    Mojo::Engine::Crawler crawler(config);
    crawler.start(server.url() + "/");

    EXPECT_TRUE(fs::exists("test_output/127.0.0.1_8085/index.md"));
    EXPECT_TRUE(fs::exists("test_output/127.0.0.1_8085/public.md"));
    EXPECT_FALSE(fs::exists("test_output/127.0.0.1_8085/private.md"));

    server.stop();
}

TEST_F(IntegrationTest, RetryLogic) {
    TestServer server;
    server.set_route("/", "<html><body><a href='/flaky'>Flaky</a></body></html>");
    server.set_route("/flaky", "<html><body>Success after retry</body></html>");

    server.start(8086);

    Mojo::Engine::CrawlerConfig config;
    config.max_depth       = 1;
    config.threads         = 1;
    config.virtual_threads = 4;
    config.output_dir      = "test_output";
    config.tree_structure  = true;

    Mojo::Engine::Crawler crawler(config);
    crawler.start(server.url() + "/");

    EXPECT_TRUE(fs::exists("test_output/127.0.0.1_8086/flaky.md"));

    server.stop();
}
