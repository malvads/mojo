#pragma once
#include <string>
#include <vector>

namespace Mojo {
namespace Storage {

class Storage {
public:
    virtual ~Storage() = default;

    virtual void
    save(const std::string& key, const std::string& content, bool is_binary = false) = 0;
};

}  // namespace Storage
}  // namespace Mojo
