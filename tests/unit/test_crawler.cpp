#include <gtest/gtest.h>
#include "../../src/core/config/config.hpp"
#include "../../src/engine/crawler/crawler.hpp"

using namespace Mojo::Engine;
using namespace Mojo::Core;

class CrawlerTest : public ::testing::Test {
protected:
    CrawlerConfig get_default_config() {
        CrawlerConfig cfg;
        cfg.max_depth      = 2;
        cfg.threads        = 1;
        cfg.output_dir     = "test_out";
        cfg.tree_structure = true;
        return cfg;
    }
};

TEST_F(CrawlerTest, ConfigMapping) {
    auto cfg      = get_default_config();
    cfg.max_depth = 5;
    Crawler crawler(cfg);
    EXPECT_EQ(crawler.max_depth_, 5);
}

TEST_F(CrawlerTest, AddUrlDepthCheck) {
    auto cfg      = get_default_config();
    cfg.max_depth = 1;
    Crawler crawler(cfg);

    crawler.start_domain_ = "example.com";

    // Depth 0: OK
    crawler.add_url("http://example.com/a", 0);
    EXPECT_EQ(crawler.frontier_.size(), 1);

    // Depth 1: OK
    crawler.add_url("http://example.com/b", 1);
    EXPECT_EQ(crawler.frontier_.size(), 2);

    // Depth 2: Should be ignored (max_depth is 1)
    crawler.add_url("http://example.com/c", 2);
    EXPECT_EQ(crawler.frontier_.size(), 2);
}

TEST_F(CrawlerTest, AddUrlDomainCheck) {
    auto    cfg = get_default_config();
    Crawler crawler(cfg);

    crawler.start_domain_ = "example.com";

    // Same domain: OK
    crawler.add_url("http://example.com/path", 0);
    EXPECT_EQ(crawler.frontier_.size(), 1);

    // Different domain: Ignored
    crawler.add_url("http://google.com/path", 0);
    EXPECT_EQ(crawler.frontier_.size(), 1);
}
