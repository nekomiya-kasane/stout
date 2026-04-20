#include "stout/util/endian.h"

#include <array>
#include <gtest/gtest.h>

using namespace stout::util;

TEST(EndianTest, ReadU16LE) {
    std::array<uint8_t, 2> buf = {0x34, 0x12};
    EXPECT_EQ(read_u16_le(std::span<const uint8_t, 2>{buf}), 0x1234);
}

TEST(EndianTest, ReadU32LE) {
    std::array<uint8_t, 4> buf = {0x78, 0x56, 0x34, 0x12};
    EXPECT_EQ(read_u32_le(std::span<const uint8_t, 4>{buf}), 0x12345678u);
}

TEST(EndianTest, ReadU64LE) {
    std::array<uint8_t, 8> buf = {0xEF, 0xCD, 0xAB, 0x90, 0x78, 0x56, 0x34, 0x12};
    EXPECT_EQ(read_u64_le(std::span<const uint8_t, 8>{buf}), 0x1234567890ABCDEFull);
}

TEST(EndianTest, WriteU16LE) {
    std::array<uint8_t, 2> buf = {};
    write_u16_le(std::span<uint8_t, 2>{buf}, 0x1234);
    EXPECT_EQ(buf[0], 0x34);
    EXPECT_EQ(buf[1], 0x12);
}

TEST(EndianTest, WriteU32LE) {
    std::array<uint8_t, 4> buf = {};
    write_u32_le(std::span<uint8_t, 4>{buf}, 0x12345678u);
    EXPECT_EQ(buf[0], 0x78);
    EXPECT_EQ(buf[1], 0x56);
    EXPECT_EQ(buf[2], 0x34);
    EXPECT_EQ(buf[3], 0x12);
}

TEST(EndianTest, WriteU64LE) {
    std::array<uint8_t, 8> buf = {};
    write_u64_le(std::span<uint8_t, 8>{buf}, 0x1234567890ABCDEFull);
    EXPECT_EQ(buf[0], 0xEF);
    EXPECT_EQ(buf[1], 0xCD);
    EXPECT_EQ(buf[2], 0xAB);
    EXPECT_EQ(buf[3], 0x90);
    EXPECT_EQ(buf[4], 0x78);
    EXPECT_EQ(buf[5], 0x56);
    EXPECT_EQ(buf[6], 0x34);
    EXPECT_EQ(buf[7], 0x12);
}

TEST(EndianTest, RoundtripU16) {
    std::array<uint8_t, 2> buf = {};
    write_u16_le(std::span<uint8_t, 2>{buf}, 0xBEEF);
    EXPECT_EQ(read_u16_le(std::span<const uint8_t, 2>{buf}), 0xBEEF);
}

TEST(EndianTest, RoundtripU32) {
    std::array<uint8_t, 4> buf = {};
    write_u32_le(std::span<uint8_t, 4>{buf}, 0xDEADBEEF);
    EXPECT_EQ(read_u32_le(std::span<const uint8_t, 4>{buf}), 0xDEADBEEF);
}

TEST(EndianTest, RoundtripU64) {
    std::array<uint8_t, 8> buf = {};
    write_u64_le(std::span<uint8_t, 8>{buf}, 0xCAFEBABEDEADBEEFull);
    EXPECT_EQ(read_u64_le(std::span<const uint8_t, 8>{buf}), 0xCAFEBABEDEADBEEFull);
}

TEST(EndianTest, ZeroValues) {
    std::array<uint8_t, 8> buf = {};
    EXPECT_EQ(read_u16_le(std::span<const uint8_t, 2>{buf.data(), 2}), 0);
    EXPECT_EQ(read_u32_le(std::span<const uint8_t, 4>{buf.data(), 4}), 0u);
    EXPECT_EQ(read_u64_le(std::span<const uint8_t, 8>{buf}), 0ull);
}

TEST(EndianTest, MaxValues) {
    std::array<uint8_t, 2> buf16 = {0xFF, 0xFF};
    EXPECT_EQ(read_u16_le(std::span<const uint8_t, 2>{buf16}), 0xFFFF);

    std::array<uint8_t, 4> buf32 = {0xFF, 0xFF, 0xFF, 0xFF};
    EXPECT_EQ(read_u32_le(std::span<const uint8_t, 4>{buf32}), 0xFFFFFFFF);

    std::array<uint8_t, 8> buf64 = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    EXPECT_EQ(read_u64_le(std::span<const uint8_t, 8>{buf64}), 0xFFFFFFFFFFFFFFFFull);
}

TEST(EndianTest, PointerOverloads) {
    uint8_t buf[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    EXPECT_EQ(read_u16_le(buf), 0x0201);
    EXPECT_EQ(read_u32_le(buf), 0x04030201u);
    EXPECT_EQ(read_u64_le(buf), 0x0807060504030201ull);

    uint8_t out[8] = {};
    write_u16_le(out, 0xAABB);
    EXPECT_EQ(out[0], 0xBB);
    EXPECT_EQ(out[1], 0xAA);
}
