#include <gtest/gtest.h>
#include "../../src/core/types/constants.hpp"
#include "../../src/utils/robotstxt/robotstxt.hpp"

using namespace Mojo::Utils;

TEST(RobotsTxtTest, BasicAllowDisallow) {
    std::string content = "User-agent: *\nDisallow: /private/\nAllow: /private/public\n";
    RobotsTxt   robots  = RobotsTxt::parse(content);

    std::string ua = Mojo::Core::Constants::USER_AGENT;
    EXPECT_TRUE(robots.is_allowed(ua, "/public"));
    EXPECT_FALSE(robots.is_allowed(ua, "/private/stuff"));
    EXPECT_TRUE(robots.is_allowed(ua, "/private/public"));
}

TEST(RobotsTxtTest, UserAgentMatching) {
    std::string content =
        "User-agent: Googlebot\n"
        "Disallow: /no-google/\n"
        "\n"
        "User-agent: *\n"
        "Disallow: /no-bots/\n";

    RobotsTxt robots = RobotsTxt::parse(content);

    EXPECT_FALSE(robots.is_allowed("Googlebot", "/no-google/"));
    EXPECT_TRUE(robots.is_allowed("Googlebot", "/no-bots/"));

    std::string ua = Mojo::Core::Constants::USER_AGENT;
    EXPECT_TRUE(robots.is_allowed(ua, "/no-google/"));
    EXPECT_FALSE(robots.is_allowed(ua, "/no-bots/"));
}

TEST(RobotsTxtTest, LongestMatchWins) {
    std::string content =
        "User-agent: *\n"
        "Disallow: /admin/\n"
        "Allow: /admin/public\n";

    RobotsTxt robots = RobotsTxt::parse(content);

    std::string ua = Mojo::Core::Constants::USER_AGENT;
    EXPECT_TRUE(
        robots.is_allowed(ua, "/admin/public"));  // Allow length 13 vs Disallow 7 -> Allow wins
    EXPECT_FALSE(robots.is_allowed(ua, "/admin/private"));
}

TEST(RobotsTxtTest, EmptyDisallow) {
    std::string content =
        "User-agent: *\n"
        "Disallow:\n";

    RobotsTxt   robots = RobotsTxt::parse(content);
    std::string ua     = Mojo::Core::Constants::USER_AGENT;
    EXPECT_TRUE(robots.is_allowed(ua, "/anywhere"));
}

TEST(RobotsTxtTest, CommentsAndFormatting) {
    std::string content =
        "# This is a comment\n"
        "User-agent: * # Inline comment\n"
        "Disallow: /tmp/ # Ignore tmp\n";

    RobotsTxt   robots = RobotsTxt::parse(content);
    std::string ua     = Mojo::Core::Constants::USER_AGENT;
    EXPECT_FALSE(robots.is_allowed(ua, "/tmp/file"));
    EXPECT_TRUE(robots.is_allowed(ua, "/other"));
}

TEST(RobotsTxtTest, CaseInsensitivity) {
    std::string content =
        "USER-AGENT: *\n"
        "DISALLOW: /Admin/\n";

    RobotsTxt   robots = RobotsTxt::parse(content);
    std::string ua     = Mojo::Core::Constants::USER_AGENT;
    EXPECT_FALSE(robots.is_allowed(ua, "/Admin/Login"));
    EXPECT_TRUE(robots.is_allowed(ua, "/admin/login"));
}

TEST(RobotsTxtTest, ComplexWildcards) {
    std::string content =
        "User-agent: *\n"
        "Disallow: /*.gif$\n"
        "Disallow: /private*/\n";

    RobotsTxt robots = RobotsTxt::parse(content);

    std::string ua = Mojo::Core::Constants::USER_AGENT;
    EXPECT_FALSE(robots.is_allowed(ua, "/private_stuff/"));
}
