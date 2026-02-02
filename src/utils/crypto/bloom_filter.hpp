#pragma once
#include <vector>
#include <string>
#include <mutex>
#include <cmath>
#include <stdint.h>
#include <stdint.h>
#include "../../core/types/constants.hpp"
#include "murmur3.h"

namespace Mojo {

using namespace Mojo::Core;

class BloomFilter {
public:
    explicit BloomFilter(size_t size = Constants::DEFAULT_BLOOM_FILTER_SIZE, 
                        int num_hashes = Constants::DEFAULT_BLOOM_FILTER_HASHES)
        : bits_(size, false), num_hashes_(num_hashes) {}

    void add(const std::string& key) {
        auto hashes = get_hashes(key);
        std::lock_guard<std::mutex> lock(mutex_);
        for (int i = 0; i < num_hashes_; ++i) {
            bits_[compute_hash(hashes, i) % bits_.size()] = true;
        }
    }

    bool contains(const std::string& key) {
        auto hashes = get_hashes(key);
        std::lock_guard<std::mutex> lock(mutex_);
        for (int i = 0; i < num_hashes_; ++i) {
            if (!bits_[compute_hash(hashes, i) % bits_.size()]) {
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

    std::pair<uint64_t, uint64_t> get_hashes(const std::string& key) const {
        uint64_t out[2];
        MurmurHash3_x64_128(key.data(), static_cast<int>(key.size()), 42, out);
        return {out[0], out[1]};
    }

    size_t compute_hash(const std::pair<uint64_t, uint64_t>& hashes, int i) const {
        return static_cast<size_t>(hashes.first + static_cast<uint64_t>(i) * hashes.second);
    }
};

}
