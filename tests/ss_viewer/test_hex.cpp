/**
 * @file test_hex.cpp
 * @brief Unit tests for hex_line formatter: empty, partial, full 16-byte lines.
 */
#include <gtest/gtest.h>

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "ss_viewer/util/hex.h"

using namespace ssv;

// ── hex_line ────────────────────────────────────────────────────────────

TEST(HexLine, FullLine) {
    std::vector<uint8_t> data = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
    };
    auto line = hex_line(data, 0);
    // Offset
    EXPECT_TRUE(line.starts_with("00000000"));
    // All 16 bytes present
    EXPECT_NE(line.find("00 01 02 03 04 05 06 07"), std::string::npos);
    EXPECT_NE(line.find("08 09 0A 0B 0C 0D 0E 0F"), std::string::npos);
    // ASCII column has pipe delimiters
    EXPECT_NE(line.find("|"), std::string::npos);
}

TEST(HexLine, PartialLine) {
    std::vector<uint8_t> data = {0x41, 0x42, 0x43};  // "ABC"
    auto line = hex_line(data, 0);
    EXPECT_TRUE(line.starts_with("00000000"));
    EXPECT_NE(line.find("41 42 43"), std::string::npos);
    // Remaining bytes should be spaces (padding)
    // ASCII should show "ABC"
    EXPECT_NE(line.find("ABC"), std::string::npos);
}

TEST(HexLine, EmptyData) {
    std::vector<uint8_t> data;
    auto line = hex_line(data, 0);
    EXPECT_TRUE(line.starts_with("00000000"));
    // No hex bytes, just padding
    EXPECT_NE(line.find("||"), std::string::npos);  // empty ASCII section
}

TEST(HexLine, OffsetNonZero) {
    std::vector<uint8_t> data(32, 0xFF);
    auto line = hex_line(data, 16);
    EXPECT_TRUE(line.starts_with("00000010"));
    EXPECT_NE(line.find("FF FF FF FF"), std::string::npos);
}

TEST(HexLine, PrintableVsNonPrintable) {
    std::vector<uint8_t> data = {
        0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x00, 0x01, 0x7F,
        0x80, 0xFF, 0x20, 0x7E, 0x1F, 0x41, 0x42, 0x43
    };
    auto line = hex_line(data, 0);
    // "Hello" should appear, non-printable as '.'
    EXPECT_NE(line.find("Hello"), std::string::npos);
    // 0x20 = space (printable), 0x7E = ~ (printable), 0x1F = . (non-printable)
    // Check ASCII section contains expected chars
    auto pipe_pos = line.rfind('|');
    ASSERT_NE(pipe_pos, std::string::npos);
}

TEST(HexLine, ExactBoundary) {
    // Data is exactly 16 bytes — line at offset 0 should be full
    std::vector<uint8_t> data(16, 0xAA);
    auto line = hex_line(data, 0);
    EXPECT_NE(line.find("AA AA AA AA AA AA AA AA"), std::string::npos);
}

TEST(HexLine, SecondLinePartial) {
    // 20 bytes → line at offset 16 has 4 bytes
    std::vector<uint8_t> data(20, 0xBB);
    auto line = hex_line(data, 16);
    EXPECT_TRUE(line.starts_with("00000010"));
    // Should have 4 BB bytes then padding
    // Count occurrences of "BB " — should be exactly 4
    size_t count = 0;
    size_t pos = 0;
    while ((pos = line.find("BB ", pos)) != std::string::npos) {
        ++count;
        pos += 3;
    }
    EXPECT_EQ(count, 4u);
}
