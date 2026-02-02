#include <gtest/gtest.h>
#include "../../src/utils/url/url.hpp"

using namespace Mojo::Utils::Url;
using namespace std::string_literals;

TEST(UrlTest, BasicParsing) {
    auto p = Url::parse("https://user:pass@example.com:8080/path/to/page?q=1#fragment");
    EXPECT_EQ(p.scheme, "https");
    EXPECT_TRUE(p.host.find("example.com") != std::string::npos);
    EXPECT_EQ(p.path, "/path/to/page");
}

TEST(UrlTest, EdgeCases) {
    EXPECT_EQ(Url::parse("data:text/plain,hello").scheme, "data");
    EXPECT_EQ(Url::parse("file:///tmp/test.txt").scheme, "file");
    EXPECT_EQ(Url::parse("http://localhost").host, "localhost");
    EXPECT_EQ(Url::parse("http://127.0.0.1").host, "127.0.0.1");
    EXPECT_EQ(Url::parse("http://[::1]").host, "[::1]");
}

TEST(UrlTest, RealisticUrls) {
    std::vector<std::string> urls = {
        "https://www.google.com/search?q=gtest+example&oq=gtest+example&aqs=chrome..69i57j0l5.2452j0j7&sourceid=chrome&ie=UTF-8",
        "https://en.wikipedia.org/wiki/Unit_testing",
        "https://github.com/google/googletest/blob/main/docs/primer.md",
        "https://news.ycombinator.com/item?id=25512345",
        "https://www.amazon.com/dp/B08L5M6449/ref=s9_acsd_al_bw_c2_x_0_i?pf_rd_m=ATVPDKIKX0DER&pf_rd_s=merchandised-search-2",
        "https://www.nytimes.com/2021/01/01/world/europe/brexit-uk-eu.html",
        "https://www.reddit.com/r/cpp/comments/kqjxyz/what_is_your_favorite_cpp_testing_framework/",
        "https://stackoverflow.com/questions/41910816/how-to-use-googletest-with-cmake",
        "https://medium.com/@mlowery/how-to-write-a-good-unit-test-d3132641e42c",
        "https://www.youtube.com/watch?v=1234567890"
    };
    for (const auto& url : urls) {
        auto p = Url::parse(url);
        EXPECT_FALSE(p.host.empty());
        EXPECT_FALSE(p.scheme.empty());
    }
}

TEST(UrlTest, RelativeResolveRFC) {
    std::string base = "https://example.com/a/b/c.html";
    // Standard relative
    EXPECT_EQ(Url::resolve(base, "d.html"), "https://example.com/a/b/d.html");
    // Protocol-relative
    EXPECT_EQ(Url::resolve(base, "//google.com/f.html"), "https://google.com/f.html");
    // Directory traversal
    EXPECT_EQ(Url::resolve(base, "../d.html"), "https://example.com/a/d.html");
    EXPECT_EQ(Url::resolve(base, "../../d.html"), "https://example.com/d.html");
    EXPECT_EQ(Url::resolve(base, "../../../d.html"), "https://example.com/d.html"); // Out of bounds
}

TEST(UrlTest, SameDomainStrict) {
    EXPECT_TRUE(Url::is_same_domain("https://example.com", "http://EXAMPLE.COM"));
    EXPECT_TRUE(Url::is_same_domain("https://example.com:443", "https://example.com"));
    EXPECT_FALSE(Url::is_same_domain("https://example.com", "https://sub.example.com"));
}

TEST(UrlTest, ToFilenameVariety) {
    EXPECT_EQ(Url::to_filename("https://example.com/page.html"), "example.com/page.md");
    EXPECT_EQ(Url::to_filename("https://example.com/dir/"), "example.com/dir/index.md");
    EXPECT_EQ(Url::to_filename("https://hola.com/júlio"), "hola.com/júlio.md");
}

TEST(UrlTest, ToFlatFilenameVariety) {
    EXPECT_EQ(Url::to_flat_filename("https://example.com/"), "example.com_.md");
    EXPECT_EQ(Url::to_flat_filename("https://example.com/a/b/c"), "example.com_a_b_c.md");
}

TEST(UrlTest, ImageExtensionsRealistic) {
    EXPECT_TRUE(Url::is_image("https://example.com/img.png"));
    EXPECT_TRUE(Url::is_image("https://example.com/img.JPG"));
    EXPECT_TRUE(Url::is_image("https://example.com/graph.svg"));
    EXPECT_TRUE(Url::is_image("https://example.com/photo.webp"));
    EXPECT_FALSE(Url::is_image("https://example.com/doc.pdf"));
    EXPECT_FALSE(Url::is_image("https://example.com/style.css"));
}

TEST(UrlTest, Internationalization) {
    // Punycode or UTF-8 hosts
    auto p1 = Url::parse("https://xn--dmin-moa.example.com/");
    EXPECT_EQ(p1.host, "xn--dmin-moa.example.com");
    
    // Non-ASCII in path
    auto p2 = Url::parse("https://example.com/目录");
    EXPECT_EQ(p2.path, "/目录");

    auto p3 = Url::parse("https://hola.com/júlio");
    EXPECT_EQ(p3.path, "/júlio");

    auto p4 = Url::parse("http://españita.es/café");
    EXPECT_EQ(p4.host, "españita.es");
    EXPECT_EQ(p4.path, "/café");
}

TEST(UrlTest, InternationalResolve) {
    std::string base = "https://españa.es/inicio";
    EXPECT_EQ(Url::resolve(base, "café"), "https://españa.es/café");
    EXPECT_EQ(Url::resolve(base, "/júlio"), "https://españa.es/júlio");
    EXPECT_EQ(Url::resolve(base, "../júlio"), "https://españa.es/júlio");
}

TEST(UrlTest, InternationalSameDomain) {
    EXPECT_TRUE(Url::is_same_domain("https://júlio.com", "http://júlio.com/path"));
    EXPECT_TRUE(Url::is_same_domain("https://españa.es", "https://españa.es."));
    EXPECT_FALSE(Url::is_same_domain("https://júlio.com", "https://julio.com")); // Accented vs Non-accented
}

TEST(UrlTest, InternationalHex) {
    // 'ú' is 0xC3 0xBA in UTF-8
    // "júlio" -> 0x6A 0xC3 0xBA 0x6C 0x69 0x6F
    std::string raw = "https://hola.com/j\xC3\xBAlio";
    auto p = Url::parse(raw);
    EXPECT_EQ(p.path, "/j\xC3\xBAlio");
    
    // 'ñ' is 0xC3 0xB1
    std::string raw2 = "http://espa\xC3\xB1\x61.es/";
    auto p2 = Url::parse(raw2);
    EXPECT_EQ(p2.host, "espa\xC3\xB1\x61.es");
}

TEST(UrlTest, PercentEncoding) {
    // Current impl might not decode, but it should preserve
    auto p = Url::parse("https://example.com/path%20with%20spaces?q=a%26b");
    EXPECT_EQ(p.path, "/path%20with%20spaces");
}

TEST(UrlTest, IPv6Realistic) {
    auto p1 = Url::parse("http://[2001:db8::1]:8080/");
    EXPECT_EQ(p1.host, "[2001:db8::1]");
    
    auto p2 = Url::parse("http://[fe80::1%eth0]/");
    EXPECT_EQ(p2.host, "[fe80::1%eth0]");
}

TEST(UrlTest, ResolutionDeepTraversal) {
    std::string base = "https://a/b/c/d/e";
    EXPECT_EQ(Url::resolve(base, "../../../.."), "https://a/");
    EXPECT_EQ(Url::resolve(base, "./f"), "https://a/b/c/d/f");
    EXPECT_EQ(Url::resolve(base, "../../../../../../../z"), "https://a/z"); // Above root
}

TEST(UrlTest, ResolutionQueryFragment) {
    std::string base = "https://example.com/page?q=1#frag";
    EXPECT_EQ(Url::resolve(base, "other?a=b"), "https://example.com/other?a=b");
    EXPECT_EQ(Url::resolve(base, "?new=view"), "https://example.com/page?new=view");
    EXPECT_EQ(Url::resolve(base, "#newfrag"), "https://example.com/page?q=1#newfrag");
}

TEST(UrlTest, SameDomainEdge) {
    EXPECT_TRUE(Url::is_same_domain("http://example.com", "http://example.com.")); // Trailing dot
    EXPECT_FALSE(Url::is_same_domain("http://example.com", "http://example.org"));
    EXPECT_FALSE(Url::is_same_domain("https://example.com", "https://notexample.com"));
}

TEST(UrlTest, BinaryAndExtreme) {
    // Null byte in path
    std::string null_url = "http://example.com/path\0with\0null"s;
    auto p = Url::parse(null_url);
    
    // Extreme length (100KB)
    std::string long_url = "http://example.com/" + std::string(1024 * 100, 'a');
    auto p2 = Url::parse(long_url);
    EXPECT_EQ(p2.host, "example.com");
    EXPECT_EQ(p2.path.length(), 1024 * 100 + 1);
}

TEST(UrlTest, Pathological) {
    std::string multi = "http://example.com/path??query=1##frag";
    auto p = Url::parse(multi);
    EXPECT_EQ(p.path, "/path");
    EXPECT_TRUE(p.start_url.find("?query=1##frag") != std::string::npos);
    
    std::string control = "http://example.com/path\r\n\tspace";
    auto p2 = Url::parse(control);
    EXPECT_EQ(p2.host, "example.com");
}

TEST(UrlTest, MalformedParse) {
    EXPECT_EQ(Url::parse(":::").scheme, "");
    EXPECT_EQ(Url::parse("http:///path").host, "");
    EXPECT_EQ(Url::parse("").start_url, "");
}

TEST(UrlTest, RecursePercent) {
    std::string recurse = "http://example.com/%252525";
    auto p = Url::parse(recurse);
    EXPECT_EQ(p.path, "/%252525");
    auto res = Url::resolve("http://example.com/", "%2525");
    EXPECT_EQ(res, "http://example.com/%2525");
}
