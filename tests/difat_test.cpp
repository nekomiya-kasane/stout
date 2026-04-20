#include "stout/cfb/difat.h"
#include "stout/io/memory_lock_bytes.h"

#include <gtest/gtest.h>

using namespace stout;
using namespace stout::cfb;
using namespace stout::io;

TEST(DifatTableTest, EmptyTable) {
    difat_table difat;
    EXPECT_EQ(difat.count(), 0u);
}

TEST(DifatTableTest, AddFatSector) {
    difat_table difat;
    difat.add_fat_sector(5);
    difat.add_fat_sector(10);
    EXPECT_EQ(difat.count(), 2u);
    EXPECT_EQ(difat.fat_sector_ids()[0], 5u);
    EXPECT_EQ(difat.fat_sector_ids()[1], 10u);
}

TEST(DifatTableTest, LoadFromHeaderOnly) {
    memory_lock_bytes mlb;
    mlb.set_size(4096 + 4096); // enough for header + some sectors
    sector_io sio(mlb, 512);

    // Create a header with some DIFAT entries
    auto hdr = make_default_header(cfb_version::v3);
    hdr.difat[0] = 1;
    hdr.difat[1] = 5;
    hdr.difat[2] = 10;
    // rest are freesect

    difat_table difat;
    auto result = difat.load(hdr, sio);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(difat.count(), 3u);
    EXPECT_EQ(difat.fat_sector_ids()[0], 1u);
    EXPECT_EQ(difat.fat_sector_ids()[1], 5u);
    EXPECT_EQ(difat.fat_sector_ids()[2], 10u);
}

TEST(DifatTableTest, LoadFromHeaderAllFree) {
    memory_lock_bytes mlb;
    mlb.set_size(1024);
    sector_io sio(mlb, 512);

    auto hdr = make_default_header(cfb_version::v3);
    // All DIFAT entries are freesect by default

    difat_table difat;
    auto result = difat.load(hdr, sio);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(difat.count(), 0u);
}

TEST(DifatTableTest, LoadFromHeaderFull109) {
    memory_lock_bytes mlb;
    mlb.set_size(512 * 200);
    sector_io sio(mlb, 512);

    auto hdr = make_default_header(cfb_version::v3);
    for (uint32_t i = 0; i < difat_in_header; ++i) {
        hdr.difat[i] = i + 1; // FAT sectors at 1..109
    }

    difat_table difat;
    auto result = difat.load(hdr, sio);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(difat.count(), static_cast<size_t>(difat_in_header));
    for (uint32_t i = 0; i < difat_in_header; ++i) {
        EXPECT_EQ(difat.fat_sector_ids()[i], i + 1);
    }
}

TEST(DifatTableTest, LoadWithDifatSectors) {
    memory_lock_bytes mlb;
    uint32_t ss = 512;
    // We need: header + enough sectors
    mlb.set_size(ss * 200);
    sector_io sio(mlb, ss);

    auto hdr = make_default_header(cfb_version::v3);
    // Fill header DIFAT
    for (uint32_t i = 0; i < difat_in_header; ++i) {
        hdr.difat[i] = i + 1;
    }

    // Create a DIFAT sector at sector 110
    // It contains 127 entries + 1 next-link
    uint32_t difat_sector_id = 110;
    hdr.first_difat_sector = difat_sector_id;
    hdr.total_difat_sectors = 1;

    std::vector<uint8_t> difat_buf(ss, 0);
    auto entries_per = difat_entries_per_sector(ss); // 127
    // Put 3 FAT sector IDs in the DIFAT sector
    util::write_u32_le(difat_buf.data() + 0, 200);
    util::write_u32_le(difat_buf.data() + 4, 201);
    util::write_u32_le(difat_buf.data() + 8, 202);
    // Rest are FREESECT
    for (uint32_t i = 3; i < entries_per; ++i) {
        util::write_u32_le(difat_buf.data() + i * 4, freesect);
    }
    // Next DIFAT sector link = ENDOFCHAIN
    util::write_u32_le(difat_buf.data() + entries_per * 4, endofchain);

    auto wr = sio.write_sector(difat_sector_id, difat_buf);
    ASSERT_TRUE(wr.has_value());

    difat_table difat;
    auto result = difat.load(hdr, sio);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(difat.count(), static_cast<size_t>(difat_in_header) + 3);
    EXPECT_EQ(difat.fat_sector_ids().back(), 202u);
}
