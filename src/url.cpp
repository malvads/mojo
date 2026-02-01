#include "mojo/url.hpp"
#include <regex>

namespace Mojo {

UrlParsed Url::parse(const std::string& url) {
    UrlParsed parsed;
    parsed.start_url = url;

    std::regex re(R"(^(([^:/?#]+):)?(//([^/?#]*))?([^?#]*)(\?([^#]*))?(#(.*))?)");
    std::smatch match;

    if (std::regex_match(url, match, re)) {
        parsed.scheme = match[2];
        parsed.host = match[4];
        parsed.path = match[5];
    }
    
    if (parsed.path.empty()) parsed.path = "/";
    
    return parsed;
}

std::string Url::resolve(const std::string& base, const std::string& relative) {
    if (relative.empty()) return base;
    if (relative.find("://") != std::string::npos) return relative;
    if (relative[0] == '#') return base;
    if (relative.substr(0, 7) == "mailto:") return "";
    if (relative.substr(0, 11) == "javascript:") return "";

    UrlParsed baseParsed = parse(base);
    
    std::string result = baseParsed.scheme + "://" + baseParsed.host;

    if (relative[0] == '/') {
        result += relative;
    } else {
        std::string searchPath = baseParsed.path;
        size_t lastSlash = searchPath.find_last_of('/');
        std::string dir = (lastSlash == std::string::npos) ? "/" : searchPath.substr(0, lastSlash + 1);
        result += dir + relative;
    }
    
    return result;
}

bool Url::is_same_domain(const std::string& url1, const std::string& url2) {
    UrlParsed p1 = parse(url1);
    UrlParsed p2 = parse(url2);
    return p1.host == p2.host;
}

std::string Url::to_filename(const std::string& url) {
    UrlParsed p = parse(url);
    std::string path = p.host + p.path;
    
    if (path.back() == '/') {
        path += "index";
    }
    
    size_t lastDot = path.find_last_of('.');
    size_t lastSlash = path.find_last_of('/');
    
    if (lastDot != std::string::npos && lastDot > lastSlash) {
        path = path.substr(0, lastDot) + ".md";
    } else {
        path += ".md";
    }
    
    return path;
}

std::string Url::to_flat_filename(const std::string& url) {
    UrlParsed p = parse(url);
    std::string path = p.host + p.path;
    
    // Replace slashes with underscores for flat structure
    for (char& c : path) {
        if (c == '/') c = '_';
    }
    
    // Ensure .md extension
    if (path.length() < 3 || path.substr(path.length() - 3) != ".md") {
        path += ".md";
    }
    
    return path;
}

}
