#include "stout/cfb/header.h"

#include <array>
#include <gtest/gtest.h>

using namespace stout;
using namespace stout::cfb;

TEST(HeaderTest, DefaultV3) {
    auto hdr = make_default_header(cfb_version::v3);
    EXPECT_EQ(hdr.signature, cfb::signature);
    EXPECT_EQ(hdr.major_ver, major_version_3);
    EXPECT_EQ(hdr.sector_shift, sector_shift_v3);
    EXPECT_EQ(hdr.sector_size(), 512u);
    EXPECT_TRUE(hdr.is_v3());
    EXPECT_FALSE(hdr.is_v4());
}

TEST(HeaderTest, DefaultV4) {
    auto hdr = make_default_header(cfb_version::v4);
    EXPECT_EQ(hdr.signature, cfb::signature);
    EXPECT_EQ(hdr.major_ver, major_version_4);
    EXPECT_EQ(hdr.sector_shift, sector_shift_v4);
    EXPECT_EQ(hdr.sector_size(), 4096u);
    EXPECT_FALSE(hdr.is_v3());
    EXPECT_TRUE(hdr.is_v4());
}

TEST(HeaderTest, DefaultDifatFilled) {
    auto hdr = make_default_header(cfb_version::v4);
    for (auto entry : hdr.difat) {
        EXPECT_EQ(entry, freesect);
    }
}

TEST(HeaderTest, SerializeRoundtripV3) {
    auto hdr = make_default_header(cfb_version::v3);
    hdr.total_fat_sectors = 1;
    hdr.first_dir_sector = 0;
    hdr.difat[0] = 1;

    std::array<uint8_t, header_size> buf = {};
    serialize_header(hdr, buf);

    auto parsed = parse_header(buf);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->major_ver, major_version_3);
    EXPECT_EQ(parsed->sector_shift, sector_shift_v3);
    EXPECT_EQ(parsed->total_fat_sectors, 1u);
    EXPECT_EQ(parsed->first_dir_sector, 0u);
    EXPECT_EQ(parsed->difat[0], 1u);
    EXPECT_EQ(parsed->difat[1], freesect);
}

TEST(HeaderTest, SerializeRoundtripV4) {
    auto hdr = make_default_header(cfb_version::v4);
    hdr.total_fat_sectors = 3;
    hdr.first_dir_sector = 5;
    hdr.first_mini_fat_sector = 10;
    hdr.total_mini_fat_sectors = 2;
    hdr.transaction_sig = 42;

    std::array<uint8_t, header_size> buf = {};
    serialize_header(hdr, buf);

    auto parsed = parse_header(buf);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->major_ver, major_version_4);
    EXPECT_EQ(parsed->total_fat_sectors, 3u);
    EXPECT_EQ(parsed->first_dir_sector, 5u);
    EXPECT_EQ(parsed->first_mini_fat_sector, 10u);
    EXPECT_EQ(parsed->total_mini_fat_sectors, 2u);
    EXPECT_EQ(parsed->transaction_sig, 42u);
}

TEST(HeaderTest, ParseInvalidSignature) {
    std::array<uint8_t, header_size> buf = {};
    auto result = parse_header(buf);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), error::invalid_signature);
}

TEST(HeaderTest, ParseInvalidByteOrder) {
    auto hdr = make_default_header(cfb_version::v4);
    std::array<uint8_t, header_size> buf = {};
    serialize_header(hdr, buf);
    // Corrupt byte order at offset 28
    buf[28] = 0x00;
    buf[29] = 0x00;
    auto result = parse_header(buf);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), error::invalid_header);
}

TEST(HeaderTest, ValidateV3Valid) {
    auto hdr = make_default_header(cfb_version::v3);
    auto result = validate_header(hdr);
    EXPECT_TRUE(result.has_value());
}

TEST(HeaderTest, ValidateV4Valid) {
    auto hdr = make_default_header(cfb_version::v4);
    auto result = validate_header(hdr);
    EXPECT_TRUE(result.has_value());
}

TEST(HeaderTest, ValidateInvalidVersion) {
    auto hdr = make_default_header(cfb_version::v4);
    hdr.major_ver = 5;
    auto result = validate_header(hdr);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), error::invalid_version);
}

TEST(HeaderTest, ValidateV3WrongSectorShift) {
    auto hdr = make_default_header(cfb_version::v3);
    hdr.sector_shift = sector_shift_v4;
    auto result = validate_header(hdr);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), error::invalid_sector_size);
}

TEST(HeaderTest, ValidateV4WrongSectorShift) {
    auto hdr = make_default_header(cfb_version::v4);
    hdr.sector_shift = sector_shift_v3;
    auto result = validate_header(hdr);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), error::invalid_sector_size);
}

TEST(HeaderTest, ValidateV3NonZeroDirSectors) {
    auto hdr = make_default_header(cfb_version::v3);
    hdr.total_dir_sectors = 1;
    auto result = validate_header(hdr);
    EXPECT_FALSE(result.has_value());
}

TEST(HeaderTest, SignatureBytes) {
    auto hdr = make_default_header(cfb_version::v4);
    std::array<uint8_t, header_size> buf = {};
    serialize_header(hdr, buf);
    EXPECT_EQ(buf[0], 0xD0);
    EXPECT_EQ(buf[1], 0xCF);
    EXPECT_EQ(buf[2], 0x11);
    EXPECT_EQ(buf[3], 0xE0);
    EXPECT_EQ(buf[4], 0xA1);
    EXPECT_EQ(buf[5], 0xB1);
    EXPECT_EQ(buf[6], 0x1A);
    EXPECT_EQ(buf[7], 0xE1);
}

TEST(HeaderTest, HeaderSizeIs512) {
    std::array<uint8_t, header_size> buf = {};
    EXPECT_EQ(buf.size(), 512u);
}

TEST(HeaderTest, AllDifatEntries) {
    auto hdr = make_default_header(cfb_version::v4);
    for (uint32_t i = 0; i < difat_in_header; ++i) {
        hdr.difat[i] = i;
    }
    std::array<uint8_t, header_size> buf = {};
    serialize_header(hdr, buf);
    auto parsed = parse_header(buf);
    ASSERT_TRUE(parsed.has_value());
    for (uint32_t i = 0; i < difat_in_header; ++i) {
        EXPECT_EQ(parsed->difat[i], i);
    }
}
