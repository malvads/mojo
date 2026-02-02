#pragma once

#include <optional>
#include <string>

namespace Mojo {
namespace Utils {
namespace Http {

struct Target {
    std::string host;
    int         port       = 0;
    bool        is_connect = false;
};

class Parser {
public:
    static std::optional<Target> parse_target(const std::string& data);
};

}  // namespace Http
}  // namespace Utils
}  // namespace Mojo
