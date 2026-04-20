#include "stout/types.h"
#include "stout/version.h"

#include <gtest/gtest.h>

using namespace stout;

TEST(TypesTest, ErrorMessages) {
    EXPECT_STREQ(error_message(error::ok), "success");
    EXPECT_STREQ(error_message(error::invalid_signature), "invalid compound file signature");
    EXPECT_STREQ(error_message(error::not_found), "entry not found");
    EXPECT_STREQ(error_message(error::io_error), "I/O error");
}

TEST(TypesTest, EntryTypes) {
    EXPECT_EQ(static_cast<uint8_t>(entry_type::unknown), 0x00);
    EXPECT_EQ(static_cast<uint8_t>(entry_type::storage), 0x01);
    EXPECT_EQ(static_cast<uint8_t>(entry_type::stream), 0x02);
    EXPECT_EQ(static_cast<uint8_t>(entry_type::root), 0x05);
}

TEST(TypesTest, CfbVersionValues) {
    EXPECT_EQ(static_cast<uint16_t>(cfb_version::v3), 3);
    EXPECT_EQ(static_cast<uint16_t>(cfb_version::v4), 4);
}

TEST(TypesTest, OpenOptionsDefaults) {
    open_options opts;
    EXPECT_EQ(opts.mode, open_mode::read_write);
    EXPECT_EQ(opts.create, create_mode::open_or_create);
    EXPECT_EQ(opts.version, cfb_version::v4);
    EXPECT_FALSE(opts.transacted);
}

TEST(TypesTest, EntryStatDefaults) {
    entry_stat stat;
    EXPECT_TRUE(stat.name.empty());
    EXPECT_EQ(stat.type, entry_type::unknown);
    EXPECT_EQ(stat.size, 0u);
    EXPECT_EQ(stat.state_bits, 0u);
}

TEST(TypesTest, LibraryVersion) {
    auto v = library_version();
    EXPECT_EQ(v.major, 0u);
    EXPECT_EQ(v.minor, 1u);
    EXPECT_EQ(v.patch, 0u);
}

TEST(TypesTest, LibraryVersionString) {
    auto s = library_version_string();
    EXPECT_STREQ(s, "0.1.0");
}
