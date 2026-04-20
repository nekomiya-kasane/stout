#include "stout/io/file_lock_bytes.h"
#include "stout/io/memory_lock_bytes.h"

#include <array>
#include <filesystem>
#include <gtest/gtest.h>

using namespace stout;
using namespace stout::io;

// ── memory_lock_bytes tests ────────────────────────────────────────────

TEST(MemoryLockBytesTest, EmptyInitialSize) {
    memory_lock_bytes mlb;
    EXPECT_EQ(mlb.size().value(), 0u);
}

TEST(MemoryLockBytesTest, WriteGrows) {
    memory_lock_bytes mlb;
    std::array<uint8_t, 100> data;
    data.fill(0xAB);
    auto result = mlb.write_at(0, data);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 100u);
    EXPECT_EQ(mlb.size().value(), 100u);
}

TEST(MemoryLockBytesTest, ReadBack) {
    memory_lock_bytes mlb;
    std::array<uint8_t, 4> write_data = {1, 2, 3, 4};
    mlb.write_at(0, write_data);

    std::array<uint8_t, 4> read_data = {};
    auto result = mlb.read_at(0, read_data);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 4u);
    EXPECT_EQ(read_data, write_data);
}

TEST(MemoryLockBytesTest, ReadPastEnd) {
    memory_lock_bytes mlb;
    std::array<uint8_t, 4> data = {1, 2, 3, 4};
    mlb.write_at(0, data);

    std::array<uint8_t, 10> buf = {};
    auto result = mlb.read_at(2, buf);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 2u);
    EXPECT_EQ(buf[0], 3);
    EXPECT_EQ(buf[1], 4);
}

TEST(MemoryLockBytesTest, ReadBeyondEnd) {
    memory_lock_bytes mlb;
    std::array<uint8_t, 4> buf = {};
    auto result = mlb.read_at(100, buf);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 0u);
}

TEST(MemoryLockBytesTest, WriteAtOffset) {
    memory_lock_bytes mlb;
    std::array<uint8_t, 4> data = {0xAA, 0xBB, 0xCC, 0xDD};
    mlb.write_at(100, data);
    EXPECT_EQ(mlb.size().value(), 104u);

    std::array<uint8_t, 4> buf = {};
    mlb.read_at(100, buf);
    EXPECT_EQ(buf, data);
}

TEST(MemoryLockBytesTest, SetSizeGrow) {
    memory_lock_bytes mlb;
    mlb.set_size(1000);
    EXPECT_EQ(mlb.size().value(), 1000u);
}

TEST(MemoryLockBytesTest, SetSizeShrink) {
    memory_lock_bytes mlb;
    mlb.set_size(1000);
    mlb.set_size(100);
    EXPECT_EQ(mlb.size().value(), 100u);
}

TEST(MemoryLockBytesTest, Flush) {
    memory_lock_bytes mlb;
    EXPECT_TRUE(mlb.flush().has_value());
}

TEST(MemoryLockBytesTest, ConstructFromVector) {
    std::vector<uint8_t> data = {1, 2, 3, 4, 5};
    memory_lock_bytes mlb(std::move(data));
    EXPECT_EQ(mlb.size().value(), 5u);

    std::array<uint8_t, 5> buf = {};
    mlb.read_at(0, buf);
    EXPECT_EQ(buf[0], 1);
    EXPECT_EQ(buf[4], 5);
}

// ── file_lock_bytes tests ──────────────────────────────────────────────

class FileLockBytesTest : public ::testing::Test {
  protected:
    std::filesystem::path temp_path;

    void SetUp() override {
        temp_path = std::filesystem::temp_directory_path() / "stout_test_file.bin";
        std::filesystem::remove(temp_path);
    }

    void TearDown() override { std::filesystem::remove(temp_path); }
};

TEST_F(FileLockBytesTest, CreateAndWrite) {
    auto lb = file_lock_bytes::open(temp_path, open_mode::read_write);
    ASSERT_TRUE(lb.has_value());

    std::array<uint8_t, 100> data;
    data.fill(0x42);
    auto result = lb->write_at(0, data);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 100u);
    lb->flush();
}

TEST_F(FileLockBytesTest, WriteAndReadBack) {
    {
        auto lb = file_lock_bytes::open(temp_path, open_mode::read_write);
        ASSERT_TRUE(lb.has_value());
        std::array<uint8_t, 4> data = {0xDE, 0xAD, 0xBE, 0xEF};
        lb->write_at(0, data);
        lb->flush();
    }
    {
        auto lb = file_lock_bytes::open(temp_path, open_mode::read);
        ASSERT_TRUE(lb.has_value());
        std::array<uint8_t, 4> buf = {};
        auto result = lb->read_at(0, buf);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(*result, 4u);
        EXPECT_EQ(buf[0], 0xDE);
        EXPECT_EQ(buf[1], 0xAD);
        EXPECT_EQ(buf[2], 0xBE);
        EXPECT_EQ(buf[3], 0xEF);
    }
}

TEST_F(FileLockBytesTest, Size) {
    auto lb = file_lock_bytes::open(temp_path, open_mode::read_write);
    ASSERT_TRUE(lb.has_value());
    EXPECT_EQ(lb->size().value(), 0u);

    std::array<uint8_t, 512> data = {};
    lb->write_at(0, data);
    lb->flush();
    EXPECT_EQ(lb->size().value(), 512u);
}

TEST_F(FileLockBytesTest, SetSize) {
    auto lb = file_lock_bytes::open(temp_path, open_mode::read_write);
    ASSERT_TRUE(lb.has_value());

    std::array<uint8_t, 1000> data = {};
    lb->write_at(0, data);
    lb->flush();

    lb->set_size(100);
    EXPECT_EQ(lb->size().value(), 100u);
}

TEST_F(FileLockBytesTest, OpenNonExistentReadOnly) {
    auto lb = file_lock_bytes::open(temp_path, open_mode::read);
    EXPECT_FALSE(lb.has_value());
}
