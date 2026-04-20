#include "stout/cfb/constants.h"

#include <gtest/gtest.h>

using namespace stout::cfb;

TEST(ConstantsTest, Signature) {
    EXPECT_EQ(signature.size(), 8u);
    EXPECT_EQ(signature[0], 0xD0);
    EXPECT_EQ(signature[7], 0xE1);
}

TEST(ConstantsTest, ByteOrder) {
    EXPECT_EQ(byte_order_le, 0xFFFE);
}

TEST(ConstantsTest, Versions) {
    EXPECT_EQ(major_version_3, 3);
    EXPECT_EQ(major_version_4, 4);
    EXPECT_EQ(minor_version, 0x003E);
}

TEST(ConstantsTest, SectorSizes) {
    EXPECT_EQ(sector_size_v3, 512u);
    EXPECT_EQ(sector_size_v4, 4096u);
    EXPECT_EQ(1u << sector_shift_v3, sector_size_v3);
    EXPECT_EQ(1u << sector_shift_v4, sector_size_v4);
}

TEST(ConstantsTest, MiniStream) {
    EXPECT_EQ(mini_sector_size, 64u);
    EXPECT_EQ(1u << mini_sector_shift, mini_sector_size);
    EXPECT_EQ(mini_stream_cutoff, 4096u);
}

TEST(ConstantsTest, SpecialSectorIDs) {
    EXPECT_EQ(difsect, 0xFFFFFFFCu);
    EXPECT_EQ(fatsect, 0xFFFFFFFDu);
    EXPECT_EQ(endofchain, 0xFFFFFFFEu);
    EXPECT_EQ(freesect, 0xFFFFFFFFu);
    EXPECT_EQ(nostream, 0xFFFFFFFFu);
}

TEST(ConstantsTest, DirectoryEntry) {
    EXPECT_EQ(dir_entry_size, 128u);
    EXPECT_EQ(dir_name_max_bytes, 64u);
    EXPECT_EQ(dir_name_max_chars, 32u);
}

TEST(ConstantsTest, Header) {
    EXPECT_EQ(header_size, 512u);
    EXPECT_EQ(difat_in_header, 109u);
}

TEST(ConstantsTest, DirEntriesPerSector) {
    EXPECT_EQ(dir_entries_per_sector(512), 4u);
    EXPECT_EQ(dir_entries_per_sector(4096), 32u);
}

TEST(ConstantsTest, FatEntriesPerSector) {
    EXPECT_EQ(fat_entries_per_sector(512), 128u);
    EXPECT_EQ(fat_entries_per_sector(4096), 1024u);
}

TEST(ConstantsTest, DifatEntriesPerSector) {
    EXPECT_EQ(difat_entries_per_sector(512), 127u);
    EXPECT_EQ(difat_entries_per_sector(4096), 1023u);
}
