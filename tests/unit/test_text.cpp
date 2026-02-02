#include <gtest/gtest.h>
#include "../../src/utils/text/converter.hpp"
#include <set>

using namespace Mojo::Utils::Text;

TEST(TextTest, LinkExtractionRealistic) {
    std::string html = R"html(
        <div>
            <a href="https://example.com/1">Link 1</a>
            <p>Some text with <a href="/internal">internal link</a></p>
            <a href="javascript:void(0)">JS link</a>
            <a href="mailto:test@example.com">Mail link</a>
            <a class="no-href">No href</a>
            <a href="">Empty href</a>
            <nav>
                <ul>
                    <li><a href="https://extern.com">Extern</a></li>
                </ul>
            </nav>
            <footer>
                <a href="#top">Anchor</a>
            </footer>
        </div>
    )html";
    auto links = Converter::extract_links(html);
    
    std::set<std::string> link_set(links.begin(), links.end());
    EXPECT_TRUE(link_set.count("https://example.com/1"));
    EXPECT_TRUE(link_set.count("/internal"));
    EXPECT_TRUE(link_set.count("javascript:void(0)"));
    EXPECT_TRUE(link_set.count("mailto:test@example.com"));
    EXPECT_TRUE(link_set.count("https://extern.com"));
    EXPECT_TRUE(link_set.count("#top"));
    EXPECT_TRUE(link_set.count("")); // Empty href
}

TEST(TextTest, CaseInsensitiveTags) {
    std::string html = "<A HREF='HTTP://UPPER.COM'>Upper</A>";
    auto links = Converter::extract_links(html);
    ASSERT_EQ(links.size(), 1);
    EXPECT_EQ(links[0], "HTTP://UPPER.COM");
}

TEST(TextTest, MarkdownConversionComplex) {
    std::string html = R"html(
        <h1>Main Title</h1>
        <p>This is <b>bold</b> and <i>italic</i>.</p>
        <ul>
            <li>Item 1</li>
            <li>Item 2 with <a href="http://link.com">link</a></li>
        </ul>
        <table>
            <tr><th>Head 1</th><th>Head 2</th></tr>
            <tr><td>Data 1</td><td>Data 2</td></tr>
        </table>
        <pre><code>code block</code></pre>
        <blockquote>Quote here</blockquote>
    )html";
    std::string md = Converter::to_markdown(html);
    EXPECT_FALSE(md.empty());
    EXPECT_NE(md.find("# Main Title"), std::string::npos);
    EXPECT_NE(md.find("**bold**"), std::string::npos);
    EXPECT_NE(md.find("*italic*"), std::string::npos);
}

TEST(TextTest, MalformedHtml) {
    std::string html = "<div><a>Unclosed tag<p>nested";
    auto links = Converter::extract_links(html);
    // Gumbo is robust, should still find the <a> if it has href
    
    std::string md = Converter::to_markdown("<<<<>>>>");
    EXPECT_FALSE(md.empty()); 
}

TEST(TextTest, UnicodeHandling) {
    std::string html = "<h1>你好</h1><p>Café</p>";
    std::string md = Converter::to_markdown(html);
    EXPECT_NE(md.find("你好"), std::string::npos);
    EXPECT_NE(md.find("Café"), std::string::npos);
}

TEST(TextTest, LargeHtml) {
    std::string html = "<html><body>";
    for(int i=0; i<1000; ++i) {
        html += "<p>Paragraph " + std::to_string(i) + " with <a href='/p" + std::to_string(i) + "'>link</a></p>";
    }
    html += "</body></html>";
    
    auto links = Converter::extract_links(html);
    EXPECT_EQ(links.size(), 1000);
    
    std::string md = Converter::to_markdown(html);
    EXPECT_GT(md.size(), 10000);
}

TEST(TextTest, LinkExtractionEdge) {
    std::string html = R"html(
        <!-- <a href="http://hidden.com">Hidden</a> -->
        <a href="http://visible.com">Visible</a>
        <meta http-equiv="refresh" content="5;url=http://meta.com">
        <link rel="stylesheet" href="/style.css">
        <base href="http://base.com/">
        <area shape="rect" coords="0,0,82,126" href="http://area.com">
    )html";
    auto links = Converter::extract_links(html);
    std::set<std::string> link_set(links.begin(), links.end());
    
    EXPECT_FALSE(link_set.count("http://hidden.com")); // In comment
    EXPECT_TRUE(link_set.count("http://visible.com"));
}

TEST(TextTest, MarkdownNesting) {
    std::string html = R"html(
        <ul>
            <li>Item 1
                <table><tr><td>Nested Table</td></tr></table>
            </li>
        </ul>
    )html";
    std::string md = Converter::to_markdown(html);
    EXPECT_NE(md.find("Nested Table"), std::string::npos);
}

TEST(TextTest, UnicodeInLinks) {
    std::string html = "<a href='https://example.com/🚀'>Rocket</a>";
    auto links = Converter::extract_links(html);
    ASSERT_EQ(links.size(), 1);
    EXPECT_EQ(links[0], "https://example.com/🚀");
}

TEST(TextTest, NestingStress) {
    std::string html;
    for(int i=0; i<1000; ++i) html += "<div>";
    html += "<a href='http://leaf.com'>Leaf</a>";
    for(int i=0; i<1000; ++i) html += "</div>";
    
    // Test link extraction on deep nesting
    auto links = Converter::extract_links(html);
    ASSERT_EQ(links.size(), 1);
    EXPECT_EQ(links[0], "http://leaf.com");
    
    // Test markdown conversion on deep nesting
    std::string md = Converter::to_markdown(html);
    EXPECT_NE(md.find("Leaf"), std::string::npos);
}

TEST(TextTest, BrokenHtmlLinks) {
    // Malformed tags
    std::string html = "<a href='http://a.com' invalid-attr='val' >OK</a><a href=\"http://b.com\"\" >Broken</a>";
    auto links = Converter::extract_links(html);
    // Should ideally find at least the first one
    EXPECT_GE(links.size(), 1);
}

TEST(TextTest, LargeAttributes) {
    std::string large_href = "https://example.com/" + std::string(10000, 'a');
    std::string html = "<a href='" + large_href + "'>Large</a>";
    auto links = Converter::extract_links(html);
    ASSERT_EQ(links.size(), 1);
    EXPECT_EQ(links[0], large_href);
}
