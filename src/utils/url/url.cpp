#include "url.hpp"
#include <regex>
#include <sstream>
#include <vector>
#include "../../core/types/constants.hpp"

namespace Mojo {
namespace Utils {

UrlParsed Url::parse(const std::string& url) {
    UrlParsed parsed;
    parsed.start_url = url;

    if (url.empty()) {
        parsed.path = "/";
        return parsed;
    }

    std::string_view sv = url;

    size_t colon       = sv.find(':');
    size_t first_slash = sv.find('/');
    size_t first_q     = sv.find('?');
    size_t first_h     = sv.find('#');
    bool   has_scheme  = (colon != std::string_view::npos);
    if (has_scheme && first_slash != std::string_view::npos && colon > first_slash)
        has_scheme = false;
    if (has_scheme && first_q != std::string_view::npos && colon > first_q)
        has_scheme = false;
    if (has_scheme && first_h != std::string_view::npos && colon > first_h)
        has_scheme = false;

    if (has_scheme) {
        parsed.scheme = std::string(sv.substr(0, colon));
        sv.remove_prefix(colon + 1);
    }

    if (sv.size() >= 2 && sv[0] == '/' && sv[1] == '/') {
        sv.remove_prefix(2);
        size_t      end_auth  = sv.find_first_of("/?#");
        std::string authority = std::string(sv.substr(0, end_auth));

        if (end_auth != std::string_view::npos) {
            sv.remove_prefix(end_auth);
        }
        else {
            sv = "";
        }

        if (!authority.empty()) {
            size_t      at = authority.find_last_of('@');
            std::string host_port =
                (at != std::string::npos) ? authority.substr(at + 1) : authority;

            if (!host_port.empty() && host_port[0] == '[') {
                size_t end_bracket = host_port.find(']');
                if (end_bracket != std::string::npos) {
                    parsed.host    = host_port.substr(0, end_bracket + 1);
                    size_t p_colon = host_port.find(':', end_bracket + 1);
                    if (p_colon != std::string::npos) {
                        parsed.port = host_port.substr(p_colon + 1);
                    }
                }
                else {
                    parsed.host = host_port;
                }
            }
            else {
                size_t p_colon = host_port.find_last_of(':');
                if (p_colon != std::string::npos) {
                    parsed.host = host_port.substr(0, p_colon);
                    parsed.port = host_port.substr(p_colon + 1);
                }
                else {
                    parsed.host = host_port;
                }
            }
        }
    }

    size_t q_pos = sv.find('?');
    size_t h_pos = sv.find('#');

    size_t path_end = sv.length();
    if (q_pos != std::string_view::npos)
        path_end = std::min(path_end, q_pos);
    if (h_pos != std::string_view::npos)
        path_end = std::min(path_end, h_pos);

    parsed.path = std::string(sv.substr(0, path_end));

    if (parsed.path.empty())
        parsed.path = "/";
    return parsed;
}

std::string Url::resolve(const std::string& base, const std::string& relative) {
    if (relative.empty())
        return base;

    if (relative[0] == '#') {
        size_t frag = base.find('#');
        if (frag == std::string::npos)
            return base + relative;
        return base.substr(0, frag) + relative;
    }

    if (relative[0] == '?') {
        size_t      q = base.find('?');
        size_t      f = base.find('#');
        std::string res;
        if (q != std::string::npos)
            res = base.substr(0, q);
        else if (f != std::string::npos)
            res = base.substr(0, f);
        else
            res = base;

        if (relative.find('#') == std::string::npos && f != std::string::npos
            && f > (q != std::string::npos ? q : 0)) {
        }
        return res + relative;
    }

    if (relative.find("://") != std::string::npos)
        return relative;

    size_t colon_pos = relative.find(':');
    if (colon_pos != std::string::npos && colon_pos < 10)
        return "";

    UrlParsed   baseParsed = parse(base);
    std::string result;

    if (relative.substr(0, 2) == "//") {
        return baseParsed.scheme + ":" + relative;
    }

    if (relative[0] == '/') {
        std::string auth = baseParsed.host;
        if (!baseParsed.port.empty())
            auth += ":" + baseParsed.port;
        result = baseParsed.scheme + "://" + auth + relative;
    }
    else {
        std::string dir = baseParsed.path;
        size_t      q   = dir.find('?');
        if (q != std::string::npos)
            dir = dir.substr(0, q);
        size_t f = dir.find('#');
        if (f != std::string::npos)
            dir = dir.substr(0, f);

        size_t lastSlash = dir.find_last_of('/');
        if (lastSlash != std::string::npos) {
            dir = dir.substr(0, lastSlash + 1);
        }
        else {
            dir = "/";
        }
        std::string auth = baseParsed.host;
        if (!baseParsed.port.empty())
            auth += ":" + baseParsed.port;
        result = baseParsed.scheme + "://" + auth + dir + relative;
    }

    size_t scheme_end = result.find("://");
    size_t domain_end = (scheme_end == std::string::npos) ? 0 : result.find('/', scheme_end + 3);
    if (domain_end == std::string::npos)
        domain_end = result.length();

    std::string path = result.substr(domain_end);
    std::string query_frag;
    size_t      qf = path.find_first_of("?#");
    if (qf != std::string::npos) {
        query_frag = path.substr(qf);
        path       = path.substr(0, qf);
    }

    std::vector<std::string> segments;
    std::stringstream        ss(path);
    std::string              segment;
    while (std::getline(ss, segment, '/')) {
        if (segment == "." || segment.empty())
            continue;
        if (segment == "..") {
            if (!segments.empty())
                segments.pop_back();
            continue;
        }
        segments.push_back(segment);
    }

    std::string normalized_path = "/";
    for (size_t i = 0; i < segments.size(); ++i) {
        normalized_path += segments[i];
        if (i < segments.size() - 1)
            normalized_path += "/";
    }
    if (path.length() > 1 && path.back() == '/' && normalized_path.back() != '/') {
        normalized_path += "/";
    }

    return result.substr(0, domain_end) + normalized_path + query_frag;
}

bool Url::is_same_domain(const std::string& url1, const std::string& url2) {
    UrlParsed p1 = parse(url1);
    UrlParsed p2 = parse(url2);

    auto clean_host = [](std::string h) {
        if (!h.empty() && h.back() == '.')
            h.pop_back();  // Strip trailing dot
        for (char& c : h) {
            if (c >= 'A' && c <= 'Z')
                c = (char)(c + ('a' - 'A'));
        }
        return h;
    };

    return clean_host(p1.host) == clean_host(p2.host);
}

std::string Url::to_filename(const std::string& url) {
    UrlParsed   p    = parse(url);
    std::string path = p.host;
    if (!p.port.empty())
        path += "_" + p.port;
    path += p.path;

    if (path.back() == '/') {
        path += "index";
    }

    size_t lastDot   = path.find_last_of('.');
    size_t lastSlash = path.find_last_of('/');

    if (lastDot != std::string::npos && lastDot > lastSlash) {
        path = path.substr(0, lastDot) + ".md";
    }
    else {
        path += ".md";
    }

    return path;
}

std::string Url::to_flat_filename(const std::string& url) {
    UrlParsed   p    = parse(url);
    std::string path = p.host;
    if (!p.port.empty())
        path += "_" + p.port;
    path += p.path;

    for (char& c : path) {
        if (c == '/')
            c = '_';
    }

    if (path.length() < 3 || path.substr(path.length() - 3) != ".md") {
        path += ".md";
    }

    return path;
}

bool Url::is_image(const std::string& url) {
    return Mojo::Core::has_extension(url, Mojo::Core::get_image_extensions());
}

}  // namespace Utils
}  // namespace Mojo
