#include <gtest/gtest.h>
#include "stout/compound_file.h"

using namespace stout;

// ── compound_file creation ─────────────────────────────────────────────

TEST(CompoundFileTest, CreateInMemoryV3) {
    auto result = compound_file::create_in_memory(cfb_version::v3);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->version(), cfb_version::v3);
    EXPECT_NE(result->data(), nullptr);
}

TEST(CompoundFileTest, CreateInMemoryV4) {
    auto result = compound_file::create_in_memory(cfb_version::v4);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->version(), cfb_version::v4);
}

TEST(CompoundFileTest, RootStorage) {
    auto cf = compound_file::create_in_memory(cfb_version::v3);
    ASSERT_TRUE(cf.has_value());
    auto root = cf->root_storage();
    EXPECT_EQ(root.name(), "Root Entry");
}

TEST(CompoundFileTest, RootStorageChildren) {
    auto cf = compound_file::create_in_memory(cfb_version::v3);
    ASSERT_TRUE(cf.has_value());
    auto root = cf->root_storage();
    auto children = root.children();
    EXPECT_TRUE(children.empty());
}

// ── storage operations ─────────────────────────────────────────────────

TEST(StorageTest, CreateStream) {
    auto cf = compound_file::create_in_memory(cfb_version::v3);
    ASSERT_TRUE(cf.has_value());
    auto root = cf->root_storage();

    auto s = root.create_stream("TestStream");
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->name(), "TestStream");
    EXPECT_EQ(s->size(), 0u);
}

TEST(StorageTest, OpenStream) {
    auto cf = compound_file::create_in_memory(cfb_version::v3);
    ASSERT_TRUE(cf.has_value());
    auto root = cf->root_storage();

    root.create_stream("MyStream");
    auto s = root.open_stream("MyStream");
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->name(), "MyStream");
}

TEST(StorageTest, OpenStreamNotFound) {
    auto cf = compound_file::create_in_memory(cfb_version::v3);
    ASSERT_TRUE(cf.has_value());
    auto root = cf->root_storage();

    auto s = root.open_stream("NonExistent");
    ASSERT_FALSE(s.has_value());
    EXPECT_EQ(s.error(), error::not_found);
}

TEST(StorageTest, CreateSubStorage) {
    auto cf = compound_file::create_in_memory(cfb_version::v3);
    ASSERT_TRUE(cf.has_value());
    auto root = cf->root_storage();

    auto sub = root.create_storage("SubFolder");
    ASSERT_TRUE(sub.has_value());
    EXPECT_EQ(sub->name(), "SubFolder");
}

TEST(StorageTest, OpenSubStorage) {
    auto cf = compound_file::create_in_memory(cfb_version::v3);
    ASSERT_TRUE(cf.has_value());
    auto root = cf->root_storage();

    root.create_storage("Folder1");
    auto sub = root.open_storage("Folder1");
    ASSERT_TRUE(sub.has_value());
    EXPECT_EQ(sub->name(), "Folder1");
}

TEST(StorageTest, Exists) {
    auto cf = compound_file::create_in_memory(cfb_version::v3);
    ASSERT_TRUE(cf.has_value());
    auto root = cf->root_storage();

    EXPECT_FALSE(root.exists("Foo"));
    root.create_stream("Foo");
    EXPECT_TRUE(root.exists("Foo"));
}

TEST(StorageTest, EnumerateChildren) {
    auto cf = compound_file::create_in_memory(cfb_version::v3);
    ASSERT_TRUE(cf.has_value());
    auto root = cf->root_storage();

    root.create_stream("Alpha");
    root.create_stream("Beta");
    root.create_storage("Gamma");

    auto children = root.children();
    EXPECT_EQ(children.size(), 3u);
}

TEST(StorageTest, RemoveStream) {
    auto cf = compound_file::create_in_memory(cfb_version::v3);
    ASSERT_TRUE(cf.has_value());
    auto root = cf->root_storage();

    root.create_stream("ToDelete");
    EXPECT_TRUE(root.exists("ToDelete"));

    auto r = root.remove("ToDelete");
    ASSERT_TRUE(r.has_value());
    EXPECT_FALSE(root.exists("ToDelete"));
}

TEST(StorageTest, RemoveNotFound) {
    auto cf = compound_file::create_in_memory(cfb_version::v3);
    ASSERT_TRUE(cf.has_value());
    auto root = cf->root_storage();

    auto r = root.remove("Ghost");
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), error::not_found);
}

TEST(StorageTest, NestedStorage) {
    auto cf = compound_file::create_in_memory(cfb_version::v3);
    ASSERT_TRUE(cf.has_value());
    auto root = cf->root_storage();

    auto sub = root.create_storage("Level1");
    ASSERT_TRUE(sub.has_value());

    auto inner = sub->create_stream("InnerStream");
    ASSERT_TRUE(inner.has_value());
    EXPECT_EQ(inner->name(), "InnerStream");

    // Verify from root
    auto sub2 = root.open_storage("Level1");
    ASSERT_TRUE(sub2.has_value());
    EXPECT_TRUE(sub2->exists("InnerStream"));
}

// ── stream operations ──────────────────────────────────────────────────

TEST(StreamTest, WriteAndRead) {
    auto cf = compound_file::create_in_memory(cfb_version::v3);
    ASSERT_TRUE(cf.has_value());
    auto root = cf->root_storage();

    auto s = root.create_stream("Data");
    ASSERT_TRUE(s.has_value());

    std::vector<uint8_t> write_data = {0x01, 0x02, 0x03, 0x04, 0x05};
    auto wr = s->write(0, write_data);
    ASSERT_TRUE(wr.has_value());
    EXPECT_EQ(*wr, 5u);
    EXPECT_EQ(s->size(), 5u);

    std::vector<uint8_t> read_data(5, 0);
    auto rd = s->read(0, read_data);
    ASSERT_TRUE(rd.has_value());
    EXPECT_EQ(*rd, 5u);
    EXPECT_EQ(read_data, write_data);
}

TEST(StreamTest, ReadBeyondEnd) {
    auto cf = compound_file::create_in_memory(cfb_version::v3);
    ASSERT_TRUE(cf.has_value());
    auto root = cf->root_storage();

    auto s = root.create_stream("Small");
    ASSERT_TRUE(s.has_value());

    std::vector<uint8_t> data = {0xAA, 0xBB};
    s->write(0, data);

    std::vector<uint8_t> buf(10, 0);
    auto rd = s->read(0, buf);
    ASSERT_TRUE(rd.has_value());
    EXPECT_EQ(*rd, 2u);
}

TEST(StreamTest, ReadAtOffset) {
    auto cf = compound_file::create_in_memory(cfb_version::v3);
    ASSERT_TRUE(cf.has_value());
    auto root = cf->root_storage();

    auto s = root.create_stream("Offset");
    ASSERT_TRUE(s.has_value());

    std::vector<uint8_t> data = {0x10, 0x20, 0x30, 0x40, 0x50};
    s->write(0, data);

    std::vector<uint8_t> buf(3, 0);
    auto rd = s->read(2, buf);
    ASSERT_TRUE(rd.has_value());
    EXPECT_EQ(*rd, 3u);
    EXPECT_EQ(buf[0], 0x30);
    EXPECT_EQ(buf[1], 0x40);
    EXPECT_EQ(buf[2], 0x50);
}

TEST(StreamTest, Resize) {
    auto cf = compound_file::create_in_memory(cfb_version::v3);
    ASSERT_TRUE(cf.has_value());
    auto root = cf->root_storage();

    auto s = root.create_stream("Resizable");
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->size(), 0u);

    auto r = s->resize(100);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(s->size(), 100u);

    r = s->resize(50);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(s->size(), 50u);
}

TEST(StreamTest, Stat) {
    auto cf = compound_file::create_in_memory(cfb_version::v3);
    ASSERT_TRUE(cf.has_value());
    auto root = cf->root_storage();

    auto s = root.create_stream("StatStream");
    ASSERT_TRUE(s.has_value());
    s->resize(42);

    auto st = s->stat();
    EXPECT_EQ(st.name, "StatStream");
    EXPECT_EQ(st.type, entry_type::stream);
    EXPECT_EQ(st.size, 42u);
}

TEST(StreamTest, MultipleStreams) {
    auto cf = compound_file::create_in_memory(cfb_version::v3);
    ASSERT_TRUE(cf.has_value());
    auto root = cf->root_storage();

    auto s1 = root.create_stream("Stream1");
    auto s2 = root.create_stream("Stream2");
    ASSERT_TRUE(s1.has_value());
    ASSERT_TRUE(s2.has_value());

    std::vector<uint8_t> d1 = {0x11, 0x22};
    std::vector<uint8_t> d2 = {0xAA, 0xBB, 0xCC};
    s1->write(0, d1);
    s2->write(0, d2);

    // Re-open and verify
    auto r1 = root.open_stream("Stream1");
    auto r2 = root.open_stream("Stream2");
    ASSERT_TRUE(r1.has_value());
    ASSERT_TRUE(r2.has_value());

    EXPECT_EQ(r1->size(), 2u);
    EXPECT_EQ(r2->size(), 3u);

    std::vector<uint8_t> buf1(2, 0), buf2(3, 0);
    r1->read(0, buf1);
    r2->read(0, buf2);
    EXPECT_EQ(buf1, d1);
    EXPECT_EQ(buf2, d2);
}

// ── roundtrip: create in memory, serialize, re-open ────────────────────

TEST(CompoundFileTest, OpenFromMemoryRoundtrip) {
    // Create a file in memory
    auto cf1 = compound_file::create_in_memory(cfb_version::v3);
    ASSERT_TRUE(cf1.has_value());

    auto root1 = cf1->root_storage();
    auto s = root1.create_stream("Hello");
    ASSERT_TRUE(s.has_value());
    std::vector<uint8_t> msg = {'H', 'e', 'l', 'l', 'o'};
    s->write(0, msg);

    // Flush
    cf1->flush();

    // Get the raw bytes
    auto* raw = cf1->data();
    ASSERT_NE(raw, nullptr);
    ASSERT_GT(raw->size(), 0u);

    // Re-open from the raw bytes
    auto cf2 = compound_file::open_from_memory(*raw);
    ASSERT_TRUE(cf2.has_value());

    auto root2 = cf2->root_storage();
    EXPECT_TRUE(root2.exists("Hello"));

    auto s2 = root2.open_stream("Hello");
    ASSERT_TRUE(s2.has_value());
    EXPECT_EQ(s2->size(), 5u);

    std::vector<uint8_t> buf(5, 0);
    s2->read(0, buf);
    EXPECT_EQ(buf, msg);
}
