#pragma once
#include <vector>
#include <string>
#include <mutex>
#include <cmath>
#include "mojo/constants.hpp"

namespace Mojo {

class BloomFilter {
public:
    explicit BloomFilter(size_t size = Constants::DEFAULT_BLOOM_FILTER_SIZE, 
                        int num_hashes = Constants::DEFAULT_BLOOM_FILTER_HASHES)
        : bits_(size, false), num_hashes_(num_hashes) {}

    void add(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (int i = 0; i < num_hashes_; ++i) {
            bits_[hash(key, i) % bits_.size()] = true;
        }
    }

    bool contains(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (int i = 0; i < num_hashes_; ++i) {
            if (!bits_[hash(key, i) % bits_.size()]) {
                return false;
            }
        }
        return true;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::fill(bits_.begin(), bits_.end(), false);
    }

private:
    std::vector<bool> bits_;
    int num_hashes_;
    std::mutex mutex_;

    size_t hash(const std::string& key, int seed) const {
        size_t h = std::hash<std::string>{}(key);
        return h ^ (static_cast<size_t>(seed) + 0x9e3779b9 + (h << 6) + (h >> 2));
    }
};

}
