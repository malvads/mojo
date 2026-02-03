#include "string_utils.hpp"
#include <algorithm>
#include <cctype>

namespace Mojo {
namespace Utils {
namespace Text {

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (std::string::npos == first) {
        return "";
    }
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

std::string to_lower(const std::string& str) {
    std::string lower = str;
    std::transform(
        lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
    return lower;
}

bool starts_with(const std::string& str, const std::string& prefix) {
    return str.rfind(prefix, 0) == 0;
}

bool ends_with(const std::string& str, const std::string& suffix) {
    if (suffix.size() > str.size())
        return false;
    return std::equal(suffix.rbegin(), suffix.rend(), str.rbegin());
}

}  // namespace Text
}  // namespace Utils
}  // namespace Mojo
