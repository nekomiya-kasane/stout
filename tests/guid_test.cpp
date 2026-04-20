#include "stout/util/guid.h"

#include <format>
#include <gtest/gtest.h>

using namespace stout;

TEST(GuidTest, NullGuid) {
    EXPECT_TRUE(guid_null.is_null());
    guid g{};
    EXPECT_TRUE(g.is_null());
}

TEST(GuidTest, NonNullGuid) {
    guid g{1, 0, 0, {}};
    EXPECT_FALSE(g.is_null());
}

TEST(GuidTest, ParseWithBraces) {
    auto g = guid_parse("{D5CDD502-2E9C-101B-9397-08002B2CF9AE}");
    ASSERT_TRUE(g.has_value());
    EXPECT_EQ(g->data1, 0xD5CDD502u);
    EXPECT_EQ(g->data2, 0x2E9C);
    EXPECT_EQ(g->data3, 0x101B);
    EXPECT_EQ(g->data4[0], 0x93);
    EXPECT_EQ(g->data4[1], 0x97);
    EXPECT_EQ(g->data4[2], 0x08);
    EXPECT_EQ(g->data4[3], 0x00);
    EXPECT_EQ(g->data4[4], 0x2B);
    EXPECT_EQ(g->data4[5], 0x2C);
    EXPECT_EQ(g->data4[6], 0xF9);
    EXPECT_EQ(g->data4[7], 0xAE);
}

TEST(GuidTest, ParseWithoutBraces) {
    auto g = guid_parse("D5CDD502-2E9C-101B-9397-08002B2CF9AE");
    ASSERT_TRUE(g.has_value());
    EXPECT_EQ(g->data1, 0xD5CDD502u);
}

TEST(GuidTest, ParseLowercase) {
    auto g = guid_parse("{d5cdd502-2e9c-101b-9397-08002b2cf9ae}");
    ASSERT_TRUE(g.has_value());
    EXPECT_EQ(g->data1, 0xD5CDD502u);
}

TEST(GuidTest, ParseInvalidTooShort) {
    auto g = guid_parse("D5CDD502-2E9C");
    EXPECT_FALSE(g.has_value());
}

TEST(GuidTest, ParseInvalidChars) {
    auto g = guid_parse("ZZZZZZZZ-2E9C-101B-9397-08002B2CF9AE");
    EXPECT_FALSE(g.has_value());
}

TEST(GuidTest, ParseEmpty) {
    auto g = guid_parse("");
    EXPECT_FALSE(g.has_value());
}

TEST(GuidTest, ParseNullGuid) {
    auto g = guid_parse("{00000000-0000-0000-0000-000000000000}");
    ASSERT_TRUE(g.has_value());
    EXPECT_TRUE(g->is_null());
}

TEST(GuidTest, ToString) {
    guid g{0xD5CDD502, 0x2E9C, 0x101B, {0x93, 0x97, 0x08, 0x00, 0x2B, 0x2C, 0xF9, 0xAE}};
    auto s = guid_to_string(g);
    EXPECT_EQ(s, "{D5CDD502-2E9C-101B-9397-08002B2CF9AE}");
}

TEST(GuidTest, RoundtripParseFormat) {
    std::string original = "{ABCDEF01-2345-6789-ABCD-EF0123456789}";
    auto g = guid_parse(original);
    ASSERT_TRUE(g.has_value());
    EXPECT_EQ(guid_to_string(*g), original);
}

TEST(GuidTest, GenerateIsV4) {
    auto g = guid_generate();
    EXPECT_FALSE(g.is_null());
    EXPECT_EQ((g.data3 >> 12) & 0xF, 4);   // version 4
    EXPECT_EQ((g.data4[0] >> 6) & 0x3, 2); // variant 1
}

TEST(GuidTest, GenerateUnique) {
    auto g1 = guid_generate();
    auto g2 = guid_generate();
    EXPECT_NE(g1, g2);
}

TEST(GuidTest, Equality) {
    guid a{1, 2, 3, {4, 5, 6, 7, 8, 9, 10, 11}};
    guid b{1, 2, 3, {4, 5, 6, 7, 8, 9, 10, 11}};
    guid c{1, 2, 3, {4, 5, 6, 7, 8, 9, 10, 12}};
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

TEST(GuidTest, Ordering) {
    guid a{1, 0, 0, {}};
    guid b{2, 0, 0, {}};
    EXPECT_LT(a, b);
    EXPECT_GT(b, a);
}

TEST(GuidTest, BinaryRoundtrip) {
    guid original{0xD5CDD502, 0x2E9C, 0x101B, {0x93, 0x97, 0x08, 0x00, 0x2B, 0x2C, 0xF9, 0xAE}};
    uint8_t buf[16] = {};
    guid_write_le(buf, original);
    auto restored = guid_read_le(buf);
    EXPECT_EQ(original, restored);
}

TEST(GuidTest, BinaryLEFormat) {
    guid g{0x01020304, 0x0506, 0x0708, {0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10}};
    uint8_t buf[16] = {};
    guid_write_le(buf, g);
    // data1 in LE
    EXPECT_EQ(buf[0], 0x04);
    EXPECT_EQ(buf[1], 0x03);
    EXPECT_EQ(buf[2], 0x02);
    EXPECT_EQ(buf[3], 0x01);
    // data2 in LE
    EXPECT_EQ(buf[4], 0x06);
    EXPECT_EQ(buf[5], 0x05);
    // data3 in LE
    EXPECT_EQ(buf[6], 0x08);
    EXPECT_EQ(buf[7], 0x07);
    // data4 raw
    EXPECT_EQ(buf[8], 0x09);
    EXPECT_EQ(buf[15], 0x10);
}

TEST(GuidTest, Formatter) {
    guid g{0xABCDEF01, 0x2345, 0x6789, {0xAB, 0xCD, 0xEF, 0x01, 0x23, 0x45, 0x67, 0x89}};
    auto s = std::format("{}", g);
    EXPECT_EQ(s, "{ABCDEF01-2345-6789-ABCD-EF0123456789}");
}
