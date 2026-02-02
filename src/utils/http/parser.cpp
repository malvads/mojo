#include "parser.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace Mojo {
namespace Utils {
namespace Http {

std::optional<Target> Parser::parse_target(const std::string& data) {
    Target target;
    if (data.rfind("CONNECT", 0) == 0) {
        target.is_connect = true;
        auto sp1          = data.find(' ');
        auto sp2          = data.find(' ', sp1 + 1);
        if (sp1 == std::string::npos || sp2 == std::string::npos)
            return std::nullopt;

        std::string host_port = data.substr(sp1 + 1, sp2 - sp1 - 1);
        auto        colon     = host_port.find(':');
        if (colon != std::string::npos) {
            target.host = host_port.substr(0, colon);
            try {
                target.port = std::stoi(host_port.substr(colon + 1));
            } catch (...) {
                return std::nullopt;
            }
        }
        else {
            target.host = host_port;
            target.port = 443;
        }
        return target;
    }

    target.is_connect = false;

    // Line-by-line processing
    std::stringstream ss(data);
    std::string       line;
    std::string       host_val;
    bool              found_host = false;

    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty())
            continue;

        // Check if line starts with Host:
        std::string lower_line = line;
        for (char& c : lower_line) {
            if (c >= 'A' && c <= 'Z')
                c = (char)(c + ('a' - 'A'));
        }

        if (lower_line.substr(0, 5) == "host:") {
            host_val = line.substr(5);
            // Trim
            size_t f = host_val.find_first_not_of(" \t");
            if (f != std::string::npos) {
                size_t l = host_val.find_last_not_of(" \t");
                host_val = host_val.substr(f, l - f + 1);
            }
            else {
                host_val = "";
            }
            found_host = true;
            break;
        }
    }

    if (!found_host)
        return std::nullopt;

    auto colon = host_val.find(':');
    if (colon != std::string::npos) {
        target.host = host_val.substr(0, colon);
        try {
            target.port = std::stoi(host_val.substr(colon + 1));
        } catch (...) {
            return std::nullopt;
        }
    }
    else {
        target.host = host_val;
        target.port = 80;
    }
    return target;
}

}  // namespace Http
}  // namespace Utils
}  // namespace Mojo
