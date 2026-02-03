#pragma once

#include <string>
#include <vector>

namespace Mojo {
namespace Utils {
namespace Text {

std::string trim(const std::string& str);
std::string to_lower(const std::string& str);
bool        starts_with(const std::string& str, const std::string& prefix);
bool        ends_with(const std::string& str, const std::string& suffix);

}  // namespace Text
}  // namespace Utils
}  // namespace Mojo
