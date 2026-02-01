#pragma once
#include <string>
#include <vector>

namespace Mojo {

class Converter {
public:
    static std::string to_markdown(const std::string& html);
    static std::vector<std::string> extract_links(const std::string& html);
};

}
