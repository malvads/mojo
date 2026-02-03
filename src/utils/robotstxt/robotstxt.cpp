/**
 * WRAPPER AROUND GOOGLE ROBOTSTXT PARSER (https://github.com/google/robotstxt)
 */
#include "robotstxt.hpp"
#include <vector>
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
    std::vector<std::string> user_agents;
    user_agents.push_back(user_agent);
    return matcher.AllowedByRobots(content_, &user_agents, path);
}

}  // namespace Utils
}  // namespace Mojo
