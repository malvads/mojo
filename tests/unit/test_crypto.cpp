#include <gtest/gtest.h>
#include <thread>
#include <unordered_set>
#include "../../src/utils/crypto/bloom_filter.hpp"
#include "../../src/utils/crypto/murmur3.h"

using namespace Mojo;

TEST(CryptoTest, BloomFilterProperties) {
    // Test with small size to observe collisions/false positives
    BloomFilter filter(100, 3);
    
    std::vector<std::string> inserted;
    for(int i=0; i<50; ++i) {
        std::string key = "key_" + std::to_string(i);
        filter.add(key);
        inserted.push_back(key);
    }
    
    // All inserted must be contained
    for(const auto& key : inserted) {
        EXPECT_TRUE(filter.contains(key));
    }
    
    // Clear and check
    filter.clear();
    for(const auto& key : inserted) {
        EXPECT_FALSE(filter.contains(key));
    }
}

TEST(CryptoTest, BloomFilterFalsePositives) {
    size_t size = 10000;
    int hashes = 7;
    BloomFilter filter(size, hashes);
    
    int num_insert = 1000;
    for(int i=0; i<num_insert; ++i) {
        filter.add("present_" + std::to_string(i));
    }
    
    int false_positives = 0;
    int num_check = 10000;
    for(int i=0; i<num_check; ++i) {
        if(filter.contains("absent_" + std::to_string(i))) {
            false_positives++;
        }
    }
    
    double fp_rate = static_cast<double>(false_positives) / num_check;
    // Theoretical FP rate for m=10000, k=7, n=1000 is approx 0.008 (0.8%)
    EXPECT_LT(fp_rate, 0.05); // Allow some margin
}

TEST(CryptoTest, BloomFilterConcurrency) {
    BloomFilter filter(10000, 5);
    std::vector<std::thread> threads;
    for(int i=0; i<10; ++i) {
        threads.emplace_back([&filter, i]() {
            for(int j=0; j<100; ++j) {
                filter.add("thread_" + std::to_string(i) + "_" + std::to_string(j));
            }
        });
    }
    for(auto& t : threads) t.join();
    
    for(int i=0; i<10; ++i) {
        for(int j=0; j<100; ++j) {
            EXPECT_TRUE(filter.contains("thread_" + std::to_string(i) + "_" + std::to_string(j)));
        }
    }
}

TEST(BloomFilterTest, FalsePositiveRateValidation) {
    BloomFilter filter(10000, 7);
    std::vector<std::string> inserted;
    for(int i=0; i<1000; ++i) {
        std::string s = "item_" + std::to_string(i);
        filter.add(s);
        inserted.push_back(s);
    }
    
    // Check inserted
    for(const auto& s : inserted) {
        EXPECT_TRUE(filter.contains(s));
    }
    
    // Check non-inserted for FPR
    int fp = 0;
    for(int i=1000; i<2000; ++i) {
        if(filter.contains("other_" + std::to_string(i))) fp++;
    }
    
    // FPR should be around 1%, and definitely not 100%
    EXPECT_LT(fp, 100); // 100/1000 = 10% safety margin for small sample
}

TEST(Murmur3Test, Consistency) {
    std::string data = "consistent data";
    uint64_t h1[2], h2[2];
    MurmurHash3_x64_128(data.c_str(), data.size(), 42, h1);
    MurmurHash3_x64_128(data.c_str(), data.size(), 42, h2);
    EXPECT_EQ(h1[0], h2[0]);
    EXPECT_EQ(h1[1], h2[1]);
}

TEST(Murmur3Test, MassHashing) {
    for(int i=0; i<10000; ++i) {
        uint64_t out[2];
        std::string s = std::to_string(i);
        MurmurHash3_x64_128(s.c_str(), s.size(), 0, out);
        // Just verify it doesn't crash and produces something
    }
}

TEST(CryptoTest, Murmur3Massive) {
    uint64_t out[2];
    std::unordered_set<uint64_t> hashes;
    for(int i=0; i<1000; ++i) {
        std::string key = "val_" + std::to_string(i);
        MurmurHash3_x64_128(key.data(), static_cast<int>(key.size()), 42, out);
        hashes.insert(out[0]);
    }
    // High probability of no collisions for 64-bit part of 128-bit hash in 1000 items
    EXPECT_EQ(hashes.size(), 1000);
}
