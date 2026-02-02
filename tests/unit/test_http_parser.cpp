#include <gtest/gtest.h>
#include "../../src/utils/http/parser.hpp"

using namespace Mojo::Utils::Http;

TEST(HttpParserTest, ParseConnectVariations) {
    auto t1 =
        Parser::parse_target("CONNECT google.com:443 HTTP/1.1\r\nHost: google.com:443\r\n\r\n");
    ASSERT_TRUE(t1.has_value());
    EXPECT_EQ(t1->host, "google.com");
    EXPECT_EQ(t1->port, 443);

    auto t2 = Parser::parse_target("CONNECT example.com HTTP/1.1\r\nHost: example.com\r\n\r\n");
    ASSERT_TRUE(t2.has_value());
    EXPECT_EQ(t2->host, "example.com");
    EXPECT_EQ(t2->port, 443);  // Default for CONNECT
}

TEST(HttpParserTest, HeaderSpacing) {
    auto t1 = Parser::parse_target("GET / HTTP/1.1\r\nHost:  spaced.com  \r\n\r\n");
    ASSERT_TRUE(t1.has_value());
    EXPECT_EQ(t1->host, "spaced.com");  // Should trim

    auto t2 = Parser::parse_target("GET / HTTP/1.1\r\nHost:portless\r\n\r\n");
    ASSERT_TRUE(t2.has_value());
    EXPECT_EQ(t2->host, "portless");
}

TEST(HttpParserTest, FoldedHeaders) {
    // Folded headers use space/tab on next line. Parser should probably skip or handle.
    auto t = Parser::parse_target(
        "GET / HTTP/1.1\r\nHost: folded.com\r\n X-Folded: oops\r\n  more\r\n\r\n");
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(t->host, "folded.com");
}

TEST(HttpParserTest, PortValidation) {
    auto t1 = Parser::parse_target("GET / HTTP/1.1\r\nHost: example.com:abc\r\n\r\n");
    EXPECT_FALSE(t1.has_value());  // Invalid port

    auto t2 = Parser::parse_target("GET / HTTP/1.1\r\nHost: example.com:65536\r\n\r\n");
    // Port 65536 is out of range, should handle gracefully
}

TEST(HttpParserTest, MultiLineMixed) {
    std::string data =
        "GET / HTTP/1.1\n"
        "User-Agent: test\r\n"
        "host: mixed-line.com\n"
        "Connection: close\r\n\r\n";
    auto t = Parser::parse_target(data);
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(t->host, "mixed-line.com");
}

TEST(HttpParserTest, AbsoluteUri) {
    // Some proxies send absolute URIs in the GET line
    auto t =
        Parser::parse_target("GET http://absolute.com/path HTTP/1.1\r\nHost: ignored.com\r\n\r\n");
    // Current parser looks at Host header, but RFC says absolute URI takes precedence.
    // Let's see what it does.
}

TEST(HttpParserTest, ParseGetVariations) {
    auto t1 = Parser::parse_target("GET / HTTP/1.1\r\nHost: example.com:8080\r\n\r\n");
    ASSERT_TRUE(t1.has_value());
    EXPECT_EQ(t1->host, "example.com");
    EXPECT_EQ(t1->port, 8080);

    auto t2 = Parser::parse_target("HEAD /favicon.ico HTTP/1.1\r\nHost: static.cc\r\n\r\n");
    ASSERT_TRUE(t2.has_value());
    EXPECT_EQ(t2->host, "static.cc");
    EXPECT_EQ(t2->port, 80);  // Default for non-CONNECT
}

TEST(HttpParserTest, MalformedData) {
    EXPECT_FALSE(Parser::parse_target("").has_value());
    EXPECT_FALSE(Parser::parse_target("GET / HTTP/1.1\r\n").has_value());  // Missing Host
    EXPECT_FALSE(Parser::parse_target("CONNECT \r\nHost: test.com\r\n\r\n")
                     .has_value());  // Missing target in CONNECT
}

TEST(HttpParserTest, LargeHeaders) {
    std::string large = "GET / HTTP/1.1\r\nHost: example.com\r\nX-Custom: ";
    large += std::string(8192, 'a');
    large += "\r\n\r\n";
    auto t = Parser::parse_target(large);
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(t->host, "example.com");
}

TEST(HttpParserTest, MixedLineEndings) {
    // Some servers/clients send mix of \n and \r\n
    auto t1 = Parser::parse_target("GET / HTTP/1.1\nHost: mixed.com\n\n");
    ASSERT_TRUE(t1.has_value());
    EXPECT_EQ(t1->host, "mixed.com");

    auto t2 = Parser::parse_target("GET / HTTP/1.1\r\nHost: mixed2.com\n\n");
    ASSERT_TRUE(t2.has_value());
    EXPECT_EQ(t2->host, "mixed2.com");
}

TEST(HttpParserTest, RobustHeaderSearch) {
    // Case insensitivity
    auto t1 = Parser::parse_target("GET / HTTP/1.1\r\nhost: lowercase.com\r\n\r\n");
    ASSERT_TRUE(t1.has_value());
    EXPECT_EQ(t1->host, "lowercase.com");

    // First line header (no leading \n)
    auto t2 = Parser::parse_target("Host: firstline.com\r\n\r\n");
    ASSERT_TRUE(t2.has_value());
    EXPECT_EQ(t2->host, "firstline.com");

    // Pure \n line endings
    auto t3 = Parser::parse_target("GET / HTTP/1.1\nHost: unix.com\n\n");
    ASSERT_TRUE(t3.has_value());
    EXPECT_EQ(t3->host, "unix.com");
}

TEST(HttpParserTest, MultipleHosts) {
    // If multiple host headers exist (invalid but happens), what do we do?
    // Current impl takes the first one it finds.
    auto t = Parser::parse_target("GET / HTTP/1.1\r\nHost: first.com\r\nHost: second.com\r\n\r\n");
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(t->host, "first.com");
}

TEST(HttpParserTest, PathologicalHeaders) {
    // Triple spaces in request line
    auto t1 = Parser::parse_target("GET   /   HTTP/1.1\r\nHost: example.com\r\n\r\n");
    ASSERT_TRUE(t1.has_value());
    EXPECT_EQ(t1->host, "example.com");

    // Ghost headers (no value)
    auto t2 = Parser::parse_target("GET / HTTP/1.1\r\nX-Empty:\r\nHost: example.com\r\n\r\n");
    ASSERT_TRUE(t2.has_value());
    EXPECT_EQ(t2->host, "example.com");
}

TEST(HttpParserTest, NonAsciiHeaders) {
    // UTF-8 in headers
    auto t1 = Parser::parse_target("GET / HTTP/1.1\r\nHost: exämple.com\r\n\r\n");
    ASSERT_TRUE(t1.has_value());
    EXPECT_EQ(t1->host, "exämple.com");
}
