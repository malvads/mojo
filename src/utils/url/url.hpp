#pragma once
#include <string>

namespace Mojo {
namespace Utils {

struct UrlParsed {
    std::string scheme;
    std::string host;
    std::string port;
    std::string path;
    std::string start_url;
};

class Url {
public:
    static UrlParsed   parse(const std::string& url);
    static std::string resolve(const std::string& base, const std::string& relative);
    static bool        is_same_domain(const std::string& url1, const std::string& url2);
    static std::string to_filename(const std::string& url);
    static std::string to_flat_filename(const std::string& url);
    static bool        is_image(const std::string& url);
};

}  // namespace Utils
}  // namespace Mojo
