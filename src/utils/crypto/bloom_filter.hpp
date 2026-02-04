#pragma once
#include <algorithm>
#include <cmath>
#include <mutex>
#include <stdint.h>
#include <string>
#include <vector>
#include "../../core/types/constants.hpp"
#include "murmur3.h"

namespace Mojo {

using namespace Mojo::Core;

class BloomFilter {
public:
    explicit BloomFilter(size_t size       = Constants::DEFAULT_BLOOM_FILTER_SIZE,
                         int    num_hashes = Constants::DEFAULT_BLOOM_FILTER_HASHES)
        : bits_(size, false), num_hashes_(num_hashes), items_added_(0) {
    }

    void add(const std::string& key) {
        auto                        hashes = get_hashes(key);
        std::lock_guard<std::mutex> lock(mutex_);
        for (int i = 0; i < num_hashes_; ++i) {
            bits_[compute_hash(hashes, i) % bits_.size()] = true;
        }
        ++items_added_;
    }

    bool contains(const std::string& key) {
        auto                        hashes = get_hashes(key);
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
        items_added_ = 0;
    }

    size_t bit_count() const {
        return bits_.size();
    }

    size_t items_added() const {
        return items_added_;
    }

    size_t set_bits() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return std::count(bits_.begin(), bits_.end(), true);
    }

    // Estimated false positive rate based on current saturation
    // Formula: (1 - e^(-kn/m))^k where k=hashes, n=items, m=bits
    double estimated_false_positive_rate() const {
        std::lock_guard<std::mutex> lock(mutex_);
        double                      k        = static_cast<double>(num_hashes_);
        double                      n        = static_cast<double>(items_added_);
        double                      m        = static_cast<double>(bits_.size());
        double                      exponent = -k * n / m;
        return std::pow(1.0 - std::exp(exponent), k);
    }

private:
    std::vector<bool>  bits_;
    int                num_hashes_;
    mutable std::mutex mutex_;
    size_t             items_added_;

    static std::pair<uint64_t, uint64_t> get_hashes(const std::string& key) {
        uint64_t out[2];
        MurmurHash3_x64_128(key.data(), static_cast<int>(key.size()), 42, out);
        return {out[0], out[1]};
    }

    static size_t compute_hash(const std::pair<uint64_t, uint64_t>& hashes, int i) {
        return static_cast<size_t>(hashes.first + static_cast<uint64_t>(i) * hashes.second);
    }
};

}  // namespace Mojo
