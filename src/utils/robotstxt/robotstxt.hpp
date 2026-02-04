/**
 * WRAPPER AROUND GOOGLE ROBOTSTXT PARSER (https://github.com/google/robotstxt)
 */
#pragma once

#include <map>
#include <string>
#include <vector>

namespace Mojo {
namespace Utils {

class RobotsTxt {
public:
    RobotsTxt() = default;

    static RobotsTxt parse(const std::string& content);

    bool   is_allowed(const std::string& user_agent, const std::string& path) const;
    double get_crawl_delay(const std::string& user_agent) const;

private:
    std::string content_;
};

}  // namespace Utils
}  // namespace Mojo
