#include <fstream>
#include <gtest/gtest.h>
#include "../../src/core/config/config.hpp"

using namespace Mojo::Core;

TEST(ConfigTest, ComplexCLI) {
    char* argv[] = {(char*)"mojo",
                    (char*)"https://test.com",
                    (char*)"--depth",
                    (char*)"10",
                    (char*)"--threads",
                    (char*)"50",
                    (char*)"--flat",
                    (char*)"--no-headless",
                    (char*)"--proxy",
                    (char*)"http://p1.com",
                    (char*)"--proxy-retries",
                    (char*)"5"};
    auto  config = Config::parse(10, argv);
    EXPECT_EQ(config.depth, 10);
    EXPECT_EQ(config.threads, 50);
}

TEST(ConfigTest, YamlLoading) {
    std::string   yaml_content = R"(
        depth: 20
        threads: 100
        output: "custom_output"
        render_js: true
        headless: false
        proxies:
          - "http://yaml_p1"
          - "http://yaml_p2"
        proxy_priorities:
          socks5: 10
          http: 5
    )";
    std::ofstream ofs("test_config.yaml");
    ofs << yaml_content;
    ofs.close();

    char* argv[] = {(char*)"mojo", (char*)"--config", (char*)"test_config.yaml"};
    auto  config = Config::parse(3, argv);

    EXPECT_EQ(config.depth, 20);
    EXPECT_EQ(config.threads, 100);
    EXPECT_EQ(config.output_dir, "custom_output");
    EXPECT_TRUE(config.render_js);
    EXPECT_FALSE(config.headless);
    EXPECT_EQ(config.proxies.size(), 2);
    EXPECT_EQ(config.proxy_priorities["socks5"], 10);

    std::remove("test_config.yaml");
}

TEST(ConfigTest, CliOverridesYaml) {
    std::string   yaml_content = "depth: 20\nthreads: 100";
    std::ofstream ofs("test_ovr.yaml");
    ofs << yaml_content;
    ofs.close();

    char* argv[] = {
        (char*)"mojo", (char*)"--config", (char*)"test_ovr.yaml", (char*)"--depth", (char*)"30"};
    auto config = Config::parse(5, argv);

    EXPECT_EQ(config.depth, 30);
    EXPECT_EQ(config.threads, 100);

    std::remove("test_ovr.yaml");
}

TEST(ConfigTest, ProxyListFile) {
    std::ofstream pfile("proxies.txt");
    pfile << "http://p1\nhttp://p2\n\nhttp://p3";
    pfile.close();

    char* argv[] = {(char*)"mojo", (char*)"--proxy-list", (char*)"proxies.txt"};
    auto  config = Config::parse(3, argv);

    EXPECT_EQ(config.proxies.size(), 3);
    EXPECT_EQ(config.proxies[0], "http://p1");

    std::remove("proxies.txt");
}
TEST(ConfigTest, InvalidYaml) {
    std::ofstream ofs("invalid.yaml");
    ofs << "depth: [not an integer]";
    ofs.close();

    const char* argv[] = {"mojo", "--config", "invalid.yaml"};
    EXPECT_THROW(Config::parse(3, (char**)argv), std::runtime_error);
    std::remove("invalid.yaml");
}

TEST(ConfigTest, NonExistentFile) {
    const char* argv[] = {"mojo", "--config", "does_not_exist.yaml"};
    EXPECT_THROW(Config::parse(3, (char**)argv), std::runtime_error);
}

TEST(ConfigTest, ProxyListRobustness) {
    std::ofstream ofs("dirty_proxies.txt");
    ofs << "http://p1:8080\n";
    ofs << "  # a comment line  \n";
    ofs << "\n";
    ofs << "http://p2:9090\n";
    ofs.close();

    const char* argv[] = {"mojo", "--proxy-list", "dirty_proxies.txt"};
    auto        config = Config::parse(3, (char**)argv);
    EXPECT_EQ(config.proxies.size(), 2);
    std::remove("dirty_proxies.txt");
}

TEST(ConfigTest, ExtremeValues) {
    char* argv[] = {(char*)"mojo", (char*)"--threads", (char*)"999999"};
    auto  config = Config::parse(3, argv);
    EXPECT_GT(config.threads, 0);
}

TEST(ConfigTest, EmptyConfig) {
    std::ofstream ofs("empty.yaml");
    ofs << "";
    ofs.close();

    char* argv[] = {(char*)"mojo", (char*)"--config", (char*)"empty.yaml"};
    auto  config = Config::parse(3, argv);
    EXPECT_EQ(config.depth, 2);

    std::remove("empty.yaml");
}
