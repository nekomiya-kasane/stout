/**
 * @file test_format.cpp
 * @brief Unit tests for format utilities: format_size, format_guid,
 *        entry_type_str, is_null_guid, is_property_stream.
 */
#include <gtest/gtest.h>

#include "ss_viewer/util/format.h"

using namespace ssv;

// ── format_size ─────────────────────────────────────────────────────────

TEST(Format, SizeBytes) { EXPECT_EQ(format_size(0), "0 B"); }
TEST(Format, SizeSmall) { EXPECT_EQ(format_size(512), "512 B"); }
TEST(Format, SizeKB) { EXPECT_EQ(format_size(1024), "1.0 KB"); }
TEST(Format, SizeKBFraction) { EXPECT_EQ(format_size(1536), "1.5 KB"); }
TEST(Format, SizeMB) { EXPECT_EQ(format_size(1048576), "1.0 MB"); }
TEST(Format, SizeGB) { EXPECT_EQ(format_size(1073741824ULL), "1.00 GB"); }
TEST(Format, SizeBoundary1023) { EXPECT_EQ(format_size(1023), "1023 B"); }

// ── format_guid ─────────────────────────────────────────────────────────

TEST(Format, GuidAllZeros) {
    stout::guid g{};
    EXPECT_EQ(format_guid(g), "{00000000-0000-0000-0000-000000000000}");
}

TEST(Format, GuidNonZero) {
    stout::guid g{};
    g.data1 = 0xDEADBEEF;
    g.data2 = 0x1234;
    g.data3 = 0x5678;
    g.data4[0] = 0xAB; g.data4[1] = 0xCD;
    g.data4[2] = 0x01; g.data4[3] = 0x02;
    g.data4[4] = 0x03; g.data4[5] = 0x04;
    g.data4[6] = 0x05; g.data4[7] = 0x06;
    EXPECT_EQ(format_guid(g), "{DEADBEEF-1234-5678-ABCD-010203040506}");
}

// ── entry_type_str ──────────────────────────────────────────────────────

TEST(Format, EntryTypeRoot) { EXPECT_EQ(entry_type_str(stout::entry_type::root), "Root Storage"); }
TEST(Format, EntryTypeStorage) { EXPECT_EQ(entry_type_str(stout::entry_type::storage), "Storage"); }
TEST(Format, EntryTypeStream) { EXPECT_EQ(entry_type_str(stout::entry_type::stream), "Stream"); }
TEST(Format, EntryTypeUnknown) { EXPECT_EQ(entry_type_str(stout::entry_type::unknown), "Unknown"); }

// ── is_null_guid ────────────────────────────────────────────────────────

TEST(Format, IsNullGuidTrue) {
    stout::guid g{};
    EXPECT_TRUE(is_null_guid(g));
}

TEST(Format, IsNullGuidFalse) {
    stout::guid g{};
    g.data1 = 1;
    EXPECT_FALSE(is_null_guid(g));
}

TEST(Format, IsNullGuidData4NonZero) {
    stout::guid g{};
    g.data4[7] = 0xFF;
    EXPECT_FALSE(is_null_guid(g));
}

// ── is_property_stream ──────────────────────────────────────────────────

TEST(Format, IsPropertyStreamTrue) {
    EXPECT_TRUE(is_property_stream("\x05SummaryInformation"));
}

TEST(Format, IsPropertyStreamFalse) {
    EXPECT_FALSE(is_property_stream("NormalStream"));
}

TEST(Format, IsPropertyStreamEmpty) {
    EXPECT_FALSE(is_property_stream(""));
}
