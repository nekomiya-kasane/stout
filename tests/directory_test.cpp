#include <gtest/gtest.h>
#include "stout/cfb/directory.h"
#include "stout/io/memory_lock_bytes.h"
#include <array>
#include <algorithm>

using namespace stout;
using namespace stout::cfb;
using namespace stout::io;

// ── dir_entry parse/serialize roundtrip ────────────────────────────────

TEST(DirEntryTest, ParseSerializeRoundtripV3) {
    dir_entry e;
    e.name = u"TestStream";
    e.type = entry_type::stream;
    e.color = node_color::black;
    e.left_sibling = 2;
    e.right_sibling = 5;
    e.child = nostream;
    e.state_bits = 0x12345678;
    e.creation_time = 0xAABBCCDDEEFF0011ULL;
    e.modified_time = 0x1122334455667788ULL;
    e.start_sector = 10;
    e.stream_size = 1000;

    std::array<uint8_t, dir_entry_size> buf = {};
    serialize_dir_entry(e, buf, true);

    auto parsed = parse_dir_entry(buf, true);
    EXPECT_EQ(parsed.name, e.name);
    EXPECT_EQ(parsed.type, e.type);
    EXPECT_EQ(parsed.color, e.color);
    EXPECT_EQ(parsed.left_sibling, e.left_sibling);
    EXPECT_EQ(parsed.right_sibling, e.right_sibling);
    EXPECT_EQ(parsed.child, e.child);
    EXPECT_EQ(parsed.state_bits, e.state_bits);
    EXPECT_EQ(parsed.creation_time, e.creation_time);
    EXPECT_EQ(parsed.modified_time, e.modified_time);
    EXPECT_EQ(parsed.start_sector, e.start_sector);
    EXPECT_EQ(parsed.stream_size, e.stream_size);
}

TEST(DirEntryTest, ParseSerializeRoundtripV4) {
    dir_entry e;
    e.name = u"BigStream";
    e.type = entry_type::stream;
    e.start_sector = 100;
    e.stream_size = 0x1'0000'0000ULL; // > 4GB, only v4

    std::array<uint8_t, dir_entry_size> buf = {};
    serialize_dir_entry(e, buf, false);

    auto parsed = parse_dir_entry(buf, false);
    EXPECT_EQ(parsed.stream_size, e.stream_size);
}

TEST(DirEntryTest, V3TruncatesStreamSize) {
    dir_entry e;
    e.name = u"Test";
    e.type = entry_type::stream;
    e.stream_size = 0x1'0000'0000ULL;

    std::array<uint8_t, dir_entry_size> buf = {};
    serialize_dir_entry(e, buf, true);

    auto parsed = parse_dir_entry(buf, true);
    EXPECT_EQ(parsed.stream_size, 0u); // truncated to 32-bit
}

TEST(DirEntryTest, EmptyEntry) {
    dir_entry e;
    EXPECT_TRUE(e.is_empty());
    EXPECT_FALSE(e.is_storage());
    EXPECT_FALSE(e.is_stream());
    EXPECT_FALSE(e.is_root());
}

TEST(DirEntryTest, RootEntry) {
    dir_entry e;
    e.type = entry_type::root;
    e.name = u"Root Entry";
    EXPECT_TRUE(e.is_root());
    EXPECT_TRUE(e.is_storage());
    EXPECT_FALSE(e.is_stream());
}

TEST(DirEntryTest, Utf8Name) {
    dir_entry e;
    e.name = u"Hello";
    EXPECT_EQ(e.utf8_name(), "Hello");
}

TEST(DirEntryTest, ToStat) {
    dir_entry e;
    e.name = u"MyStream";
    e.type = entry_type::stream;
    e.stream_size = 42;
    auto s = e.to_stat();
    EXPECT_EQ(s.name, "MyStream");
    EXPECT_EQ(s.type, entry_type::stream);
    EXPECT_EQ(s.size, 42u);
}

// ── directory class tests ──────────────────────────────────────────────

class DirectoryTest : public ::testing::Test {
protected:
    directory dir;

    void SetUp() override {
        // Create root entry (id 0)
        auto root_id = dir.add_entry();
        ASSERT_EQ(root_id, 0u);
        dir.entry(root_id).name = u"Root Entry";
        dir.entry(root_id).type = entry_type::root;
        dir.entry(root_id).color = node_color::black;
    }
};

TEST_F(DirectoryTest, RootEntry) {
    EXPECT_EQ(dir.count(), 1u);
    EXPECT_TRUE(dir.root().is_root());
    EXPECT_EQ(dir.root().utf8_name(), "Root Entry");
}

TEST_F(DirectoryTest, AddAndFindChild) {
    auto id = dir.add_entry();
    dir.entry(id).name = u"Stream1";
    dir.entry(id).type = entry_type::stream;
    dir.insert_child(0, id);

    auto found = dir.find_child(0, u"Stream1");
    EXPECT_EQ(found, id);
}

TEST_F(DirectoryTest, FindChildNotFound) {
    auto found = dir.find_child(0, u"NonExistent");
    EXPECT_EQ(found, nostream);
}

TEST_F(DirectoryTest, MultipleChildren) {
    auto id1 = dir.add_entry();
    dir.entry(id1).name = u"Alpha";
    dir.entry(id1).type = entry_type::stream;
    dir.insert_child(0, id1);

    auto id2 = dir.add_entry();
    dir.entry(id2).name = u"Beta";
    dir.entry(id2).type = entry_type::stream;
    dir.insert_child(0, id2);

    auto id3 = dir.add_entry();
    dir.entry(id3).name = u"Gamma";
    dir.entry(id3).type = entry_type::stream;
    dir.insert_child(0, id3);

    EXPECT_EQ(dir.find_child(0, u"Alpha"), id1);
    EXPECT_EQ(dir.find_child(0, u"Beta"), id2);
    EXPECT_EQ(dir.find_child(0, u"Gamma"), id3);
}

TEST_F(DirectoryTest, EnumerateChildren) {
    std::vector<std::u16string> names = {u"Delta", u"Alpha", u"Charlie", u"Beta"};
    for (auto& n : names) {
        auto id = dir.add_entry();
        dir.entry(id).name = n;
        dir.entry(id).type = entry_type::stream;
        dir.insert_child(0, id);
    }

    std::vector<std::u16string> collected;
    dir.enumerate_children(0, [&](uint32_t, const dir_entry& e) {
        collected.push_back(e.name);
    });

    // Should be sorted by CFB name ordering (in-order traversal of BST)
    ASSERT_EQ(collected.size(), 4u);
    for (size_t i = 1; i < collected.size(); ++i) {
        EXPECT_LT(util::cfb_name_compare(collected[i - 1], collected[i]), 0);
    }
}

TEST_F(DirectoryTest, RemoveChild) {
    auto id1 = dir.add_entry();
    dir.entry(id1).name = u"First";
    dir.entry(id1).type = entry_type::stream;
    dir.insert_child(0, id1);

    auto id2 = dir.add_entry();
    dir.entry(id2).name = u"Second";
    dir.entry(id2).type = entry_type::stream;
    dir.insert_child(0, id2);

    dir.remove_child(0, id1);

    EXPECT_EQ(dir.find_child(0, u"First"), nostream);
    EXPECT_EQ(dir.find_child(0, u"Second"), id2);
    EXPECT_TRUE(dir.entry(id1).is_empty());
}

TEST_F(DirectoryTest, ManyChildren) {
    // Insert 20 children to exercise the red-black tree
    for (int i = 0; i < 20; ++i) {
        auto id = dir.add_entry();
        dir.entry(id).name = std::u16string(1, static_cast<char16_t>(u'A' + i));
        dir.entry(id).type = entry_type::stream;
        dir.insert_child(0, id);
    }

    // All should be findable
    for (int i = 0; i < 20; ++i) {
        auto name = std::u16string(1, static_cast<char16_t>(u'A' + i));
        EXPECT_NE(dir.find_child(0, name), nostream) << "Failed to find: " << char('A' + i);
    }

    // Enumerate should give sorted order
    std::vector<std::string> collected;
    dir.enumerate_children(0, [&](uint32_t, const dir_entry& e) {
        collected.push_back(e.utf8_name());
    });
    ASSERT_EQ(collected.size(), 20u);
    for (size_t i = 1; i < collected.size(); ++i) {
        EXPECT_LE(collected[i - 1], collected[i]);
    }
}

TEST_F(DirectoryTest, SubStorage) {
    // Create a sub-storage
    auto storage_id = dir.add_entry();
    dir.entry(storage_id).name = u"SubStorage";
    dir.entry(storage_id).type = entry_type::storage;
    dir.insert_child(0, storage_id);

    // Add children to the sub-storage
    auto s1 = dir.add_entry();
    dir.entry(s1).name = u"InnerStream";
    dir.entry(s1).type = entry_type::stream;
    dir.insert_child(storage_id, s1);

    EXPECT_EQ(dir.find_child(0, u"SubStorage"), storage_id);
    EXPECT_EQ(dir.find_child(storage_id, u"InnerStream"), s1);
}

// ── directory load/flush roundtrip ─────────────────────────────────────

TEST(DirectoryLoadFlushTest, RoundtripV3) {
    memory_lock_bytes mlb;
    uint32_t ss = 512;
    // header + 1 dir sector + 1 FAT sector
    mlb.set_size(ss * 3);
    sector_io sio(mlb, ss);

    fat_table fat;
    fat.resize(3);
    fat.set(0, endofchain); // dir sector
    fat.set(1, endofchain); // FAT sector

    // Build a directory
    directory dir_orig;
    auto root_id = dir_orig.add_entry();
    dir_orig.entry(root_id).name = u"Root Entry";
    dir_orig.entry(root_id).type = entry_type::root;
    dir_orig.entry(root_id).color = node_color::black;

    auto s1 = dir_orig.add_entry();
    dir_orig.entry(s1).name = u"Stream1";
    dir_orig.entry(s1).type = entry_type::stream;
    dir_orig.entry(s1).stream_size = 100;
    dir_orig.entry(s1).start_sector = 2;
    dir_orig.insert_child(0, s1);

    // Flush
    auto wr = dir_orig.flush(sio, fat, 0);
    ASSERT_TRUE(wr.has_value());

    // Load back
    directory dir_loaded;
    auto rd = dir_loaded.load(sio, fat, 0, true);
    ASSERT_TRUE(rd.has_value());

    EXPECT_GE(dir_loaded.count(), 2u);
    EXPECT_EQ(dir_loaded.root().utf8_name(), "Root Entry");
    EXPECT_TRUE(dir_loaded.root().is_root());

    auto found = dir_loaded.find_child(0, u"Stream1");
    EXPECT_NE(found, nostream);
    if (found != nostream) {
        EXPECT_EQ(dir_loaded.entry(found).stream_size, 100u);
        EXPECT_EQ(dir_loaded.entry(found).start_sector, 2u);
    }
}
