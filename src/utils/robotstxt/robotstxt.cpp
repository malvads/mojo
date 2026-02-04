/**
 * WRAPPER AROUND GOOGLE ROBOTSTXT PARSER (https://github.com/google/robotstxt)
 */
#include "robotstxt.hpp"
#include <optional>
#include <vector>
#include "absl/strings/match.h"
#include "robots.h"

namespace Mojo {
namespace Utils {

RobotsTxt RobotsTxt::parse(const std::string& content) {
    RobotsTxt robots;
    robots.content_ = content;
    return robots;
}

bool RobotsTxt::is_allowed(const std::string& user_agent, const std::string& path) const {
    googlebot::RobotsMatcher matcher;
    std::vector<std::string> user_agents{user_agent};
    return matcher.AllowedByRobots(content_, &user_agents, path);
}

namespace {
class CrawlDelayMatcher : public googlebot::RobotsMatcher {
public:
    explicit CrawlDelayMatcher(const std::vector<std::string>& user_agents) {
        InitUserAgentsAndPath(&user_agents, "/");
    }

    double GetDelay(const std::string& content) {
        googlebot::ParseRobotsTxt(content, this);
        if (specific_delay_)
            return *specific_delay_;
        if (global_delay_)
            return *global_delay_;
        return 0.0;
    }

protected:
    void
    HandleUnknownAction(int line_num, absl::string_view action, absl::string_view value) override {
        if (absl::EqualsIgnoreCase(action, "Crawl-delay")) {
            try {
                double delay = std::stod(std::string(value));
                if (seen_specific_agent_)
                    specific_delay_ = delay;
                else if (seen_global_agent_)
                    global_delay_ = delay;
            } catch (...) {
            }
        }
        googlebot::RobotsMatcher::HandleUnknownAction(line_num, action, value);
    }

private:
    std::optional<double> global_delay_;
    std::optional<double> specific_delay_;
};
}  // namespace

double RobotsTxt::get_crawl_delay(const std::string& user_agent) const {
    std::vector<std::string> ua_list{user_agent};
    CrawlDelayMatcher        matcher(ua_list);
    return matcher.GetDelay(content_);
}

}  // namespace Utils
}  // namespace Mojo
