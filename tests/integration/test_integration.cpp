#include <gtest/gtest.h>
#include <httplib.h>
#include <thread>
#include <filesystem>
#include <fstream>
#include "crawler/crawler.hpp"
#include "logger/logger.hpp"

namespace fs = std::filesystem;

class TestServer {
public:
    TestServer() {}
    
    void set_route(const std::string& path, const std::string& content, const std::string& type = "text/html") {
        server_.Get(path, [content, type](const httplib::Request&, httplib::Response& res) {
            res.set_content(content, type.c_str());
        });
    }

    void start(int port) {
        port_ = port;
        thread_ = std::thread([this, port]() {
            server_.listen("127.0.0.1", port);
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    void stop() {
        server_.stop();
        if (thread_.joinable()) thread_.join();
    }

    std::string url() const {
        return "http://127.0.0.1:" + std::to_string(port_);
    }

private:
    httplib::Server server_;
    std::thread thread_;
    int port_ = 0;
};

class IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        Mojo::Core::Logger::set_level(Mojo::Core::LOG_ERROR);
        if (fs::exists("test_output")) fs::remove_all("test_output");
        fs::create_directory("test_output");
    }

    void TearDown() override {
        if (fs::exists("test_output")) fs::remove_all("test_output");
    }
};

TEST_F(IntegrationTest, BasicCrawlDiscovery) {
    TestServer server;
    server.set_route("/", "<html><body><a href='/a'>A</a></body></html>");
    server.set_route("/a", "<html><body><a href='/b'>B</a></body></html>");
    server.set_route("/b", "<html><body>Done</body></html>");
    server.start(8081);

    Mojo::Engine::CrawlerConfig config;
    config.max_depth = 2;
    config.threads = 1;
    config.output_dir = "test_output";
    config.tree_structure = true;

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
    server.set_route("/", "<html><body><img src='/logo.png'><a href='/logo.png'>Link to Image</a></body></html>");
    server.set_route("/logo.png", "FAKE_IMAGE_DATA", "image/png");
    server.start(8082);

    Mojo::Engine::CrawlerConfig config;
    config.max_depth = 1;
    config.threads = 1;
    config.output_dir = "test_output";
    config.tree_structure = true;

    Mojo::Engine::Crawler crawler(config);
    crawler.start(server.url() + "/");

    EXPECT_TRUE(fs::exists("test_output/127.0.0.1_8082/index.md"));
    EXPECT_TRUE(fs::exists("test_output/127.0.0.1_8082/logo.png"));
    EXPECT_FALSE(fs::exists("test_output/127.0.0.1_8082/logo.md"));
    
    server.stop();
}

TEST_F(IntegrationTest, DomainRestriction) {
    TestServer server1;
    server1.set_route("/", "<html><body><a href='http://127.0.0.1:8084/ext'>External</a></body></html>");
    server1.start(8083);

    TestServer server2;
    server2.set_route("/ext", "<html><body>External Page</body></html>");
    server2.start(8084);

    Mojo::Engine::CrawlerConfig config;
    config.max_depth = 1;
    config.threads = 1;
    config.output_dir = "test_output";
    config.tree_structure = true;

    Mojo::Engine::Crawler crawler(config);
    crawler.start(server1.url() + "/");

    EXPECT_TRUE(fs::exists("test_output/127.0.0.1_8083/index.md"));
    EXPECT_FALSE(fs::exists("test_output/127.0.0.1_8084/ext.md"));

    server1.stop();
    server2.stop();
}
