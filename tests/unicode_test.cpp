#include "stout/util/unicode.h"

#include <gtest/gtest.h>

using namespace stout::util;

TEST(UnicodeTest, Utf8ToUtf16Ascii) {
    auto result = utf8_to_utf16le("Hello");
    EXPECT_EQ(result.size(), 5u);
    EXPECT_EQ(result[0], u'H');
    EXPECT_EQ(result[4], u'o');
}

TEST(UnicodeTest, Utf16ToUtf8Ascii) {
    std::u16string u16 = u"Hello";
    auto result = utf16le_to_utf8(u16);
    EXPECT_EQ(result, "Hello");
}

TEST(UnicodeTest, Utf8Utf16Roundtrip) {
    std::string original = "Hello World";
    auto u16 = utf8_to_utf16le(original);
    auto back = utf16le_to_utf8(u16);
    EXPECT_EQ(back, original);
}

TEST(UnicodeTest, Utf8ToUtf16TwoByte) {
    // "ä" = U+00E4 = 0xC3 0xA4 in UTF-8
    std::string utf8 = "\xC3\xA4";
    auto u16 = utf8_to_utf16le(utf8);
    EXPECT_EQ(u16.size(), 1u);
    EXPECT_EQ(u16[0], u'\u00E4');
}

TEST(UnicodeTest, Utf8ToUtf16ThreeByte) {
    // "€" = U+20AC = 0xE2 0x82 0xAC in UTF-8
    std::string utf8 = "\xE2\x82\xAC";
    auto u16 = utf8_to_utf16le(utf8);
    EXPECT_EQ(u16.size(), 1u);
    EXPECT_EQ(u16[0], u'\u20AC');
}

TEST(UnicodeTest, Utf8ToUtf16Surrogate) {
    // U+1F600 (😀) = 0xF0 0x9F 0x98 0x80 in UTF-8 -> surrogate pair in UTF-16
    std::string utf8 = "\xF0\x9F\x98\x80";
    auto u16 = utf8_to_utf16le(utf8);
    EXPECT_EQ(u16.size(), 2u);
    EXPECT_EQ(u16[0], static_cast<char16_t>(0xD83D));
    EXPECT_EQ(u16[1], static_cast<char16_t>(0xDE00));
}

TEST(UnicodeTest, DirNameRoundtrip) {
    auto result = utf8_to_dir_name("TestStream");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->byte_count, 22u); // 10 chars + null = 11 * 2 = 22

    auto back = dir_name_to_utf8(result->bytes, result->byte_count);
    EXPECT_EQ(back, "TestStream");
}

TEST(UnicodeTest, DirNameEmpty) {
    auto result = utf8_to_dir_name("");
    EXPECT_FALSE(result.has_value());
}

TEST(UnicodeTest, DirNameTooLong) {
    std::string long_name(32, 'A'); // 32 chars, max is 31
    auto result = utf8_to_dir_name(long_name);
    EXPECT_FALSE(result.has_value());
}

TEST(UnicodeTest, DirNameMaxLength) {
    std::string max_name(31, 'A'); // exactly 31 chars
    auto result = utf8_to_dir_name(max_name);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->byte_count, 64u); // 31 chars + null = 32 * 2 = 64
}

TEST(UnicodeTest, DirNameIllegalChars) {
    EXPECT_FALSE(utf8_to_dir_name("a/b").has_value());
    EXPECT_FALSE(utf8_to_dir_name("a\\b").has_value());
    EXPECT_FALSE(utf8_to_dir_name("a:b").has_value());
    EXPECT_FALSE(utf8_to_dir_name("a!b").has_value());
}

TEST(UnicodeTest, CfbToupperAscii) {
    EXPECT_EQ(cfb_toupper(u'a'), u'A');
    EXPECT_EQ(cfb_toupper(u'z'), u'Z');
    EXPECT_EQ(cfb_toupper(u'A'), u'A');
    EXPECT_EQ(cfb_toupper(u'0'), u'0');
}

TEST(UnicodeTest, CfbToupperLatin1) {
    EXPECT_EQ(cfb_toupper(u'\u00E0'), u'\u00C0'); // à -> À
    EXPECT_EQ(cfb_toupper(u'\u00F6'), u'\u00D6'); // ö -> Ö
    EXPECT_EQ(cfb_toupper(u'\u00F8'), u'\u00D8'); // ø -> Ø
    EXPECT_EQ(cfb_toupper(u'\u00FE'), u'\u00DE'); // þ -> Þ
}

TEST(UnicodeTest, CfbNameCompareSameLength) {
    std::u16string a = u"ABC";
    std::u16string b = u"ABD";
    EXPECT_LT(cfb_name_compare(a, b), 0);
    EXPECT_GT(cfb_name_compare(b, a), 0);
    EXPECT_EQ(cfb_name_compare(a, a), 0);
}

TEST(UnicodeTest, CfbNameCompareDifferentLength) {
    std::u16string shorter = u"AB";
    std::u16string longer = u"ABC";
    EXPECT_LT(cfb_name_compare(shorter, longer), 0);
    EXPECT_GT(cfb_name_compare(longer, shorter), 0);
}

TEST(UnicodeTest, CfbNameCompareCaseInsensitive) {
    std::u16string a = u"Hello";
    std::u16string b = u"HELLO";
    EXPECT_EQ(cfb_name_compare(a, b), 0);
}

TEST(UnicodeTest, CfbNameIsValid) {
    EXPECT_TRUE(cfb_name_is_valid("Hello"));
    EXPECT_TRUE(cfb_name_is_valid("Test Stream 123"));
    EXPECT_FALSE(cfb_name_is_valid(""));
    EXPECT_FALSE(cfb_name_is_valid("a/b"));
    EXPECT_FALSE(cfb_name_is_valid("a\\b"));
    EXPECT_FALSE(cfb_name_is_valid("a:b"));
    EXPECT_FALSE(cfb_name_is_valid("a!b"));
}
