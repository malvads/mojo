#pragma once
#include <string>
#include "storage.hpp"

namespace Mojo {
namespace Storage {

class DiskStorage : public Storage {
public:
    explicit DiskStorage(const std::string& base_path);
    ~DiskStorage() override = default;

    void save(const std::string& key, const std::string& content, bool is_binary = false) override;

private:
    std::string base_path_;
};

}  // namespace Storage
}  // namespace Mojo
