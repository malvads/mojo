#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include "../../src/storage/disk_storage.hpp"

using namespace Mojo::Storage;
namespace fs = std::filesystem;

class StorageTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (fs::exists("test_storage_out"))
            fs::remove_all("test_storage_out");
    }

    void TearDown() override {
        if (fs::exists("test_storage_out"))
            fs::remove_all("test_storage_out");
    }
};

TEST_F(StorageTest, DiskStorageCreation) {
    DiskStorage storage("test_storage_out");
    storage.save("test.txt", "Hello World", false);

    EXPECT_TRUE(fs::exists("test_storage_out/test.txt"));

    std::ifstream file("test_storage_out/test.txt");
    std::string   content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    EXPECT_EQ(content, "Hello World");
}

TEST_F(StorageTest, NestedDirectoryCreation) {
    DiskStorage storage("test_storage_out");
    storage.save("deep/path/to/file.md", "# Heading", false);

    EXPECT_TRUE(fs::exists("test_storage_out/deep/path/to/file.md"));
}

TEST_F(StorageTest, BinaryStorage) {
    DiskStorage storage("test_storage_out");
    std::string binary_data = {0x00, 0x01, 0x02, 0x03, (char)0xFF};
    storage.save("data.bin", binary_data, true);

    EXPECT_TRUE(fs::exists("test_storage_out/data.bin"));

    std::ifstream file("test_storage_out/data.bin", std::ios::binary);
    std::string   content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    EXPECT_EQ(content, binary_data);
}
