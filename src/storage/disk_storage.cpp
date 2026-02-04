#include "disk_storage.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include "../core/logger/logger.hpp"

namespace Mojo {
namespace Storage {

DiskStorage::DiskStorage(const std::string& base_path) : base_path_(base_path) {
    if (!base_path_.empty()) {
        try {
            std::filesystem::create_directories(base_path_);
        } catch (...) {
            Mojo::Core::Logger::error("Failed to create storage directory: " + base_path_);
        }
    }
}

void DiskStorage::save(const std::string& key, const std::string& content, bool is_binary) {
    try {
        std::filesystem::path path(base_path_);
        path /= key;

        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path());
        }

        std::ofstream file(path, is_binary ? std::ios::binary : std::ios::out);
        if (file.is_open()) {
            file.write(content.data(), content.size());
            Mojo::Core::Logger::success("Saved: " + path.string());
        }
        else {
            Mojo::Core::Logger::error("Write Error: " + path.string());
        }
    } catch (const std::exception& e) {
        Mojo::Core::Logger::error("FS Error: " + std::string(e.what()));
    } catch (...) {
        Mojo::Core::Logger::error("FS Error: Unknown exception working with " + key);
    }
}

}  // namespace Storage
}  // namespace Mojo
