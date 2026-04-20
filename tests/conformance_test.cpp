#include <gtest/gtest.h>
#include "stout/compound_file.h"
#include "stout/ole/property_set.h"

using namespace stout;
using namespace stout::ole;

// ── End-to-end: create, populate, flush, reopen, verify ────────────────

TEST(ConformanceTest, CreateFlushReopenV3) {
    auto cf = compound_file::create_in_memory(cfb_version::v3);
    ASSERT_TRUE(cf.has_value());

    auto root = cf->root_storage();
    auto s1 = root.create_stream("Stream1");
    ASSERT_TRUE(s1.has_value());
    std::vector<uint8_t> data1(100, 0x42);
    s1->write(0, data1);

    auto sub = root.create_storage("SubStorage");
    ASSERT_TRUE(sub.has_value());
    auto s2 = sub->create_stream("InnerStream");
    ASSERT_TRUE(s2.has_value());
    std::vector<uint8_t> data2 = {0xDE, 0xAD, 0xBE, 0xEF};
    s2->write(0, data2);

    cf->flush();

    // Reopen from memory
    auto raw = *cf->data();
    auto cf2 = compound_file::open_from_memory(raw);
    ASSERT_TRUE(cf2.has_value());
    EXPECT_EQ(cf2->version(), cfb_version::v3);

    auto root2 = cf2->root_storage();
    EXPECT_TRUE(root2.exists("Stream1"));
    EXPECT_TRUE(root2.exists("SubStorage"));

    auto r1 = root2.open_stream("Stream1");
    ASSERT_TRUE(r1.has_value());
    EXPECT_EQ(r1->size(), 100u);
    std::vector<uint8_t> buf1(100, 0);
    r1->read(0, buf1);
    EXPECT_EQ(buf1, data1);

    auto rsub = root2.open_storage("SubStorage");
    ASSERT_TRUE(rsub.has_value());
    auto r2 = rsub->open_stream("InnerStream");
    ASSERT_TRUE(r2.has_value());
    EXPECT_EQ(r2->size(), 4u);
    std::vector<uint8_t> buf2(4, 0);
    r2->read(0, buf2);
    EXPECT_EQ(buf2, data2);
}

TEST(ConformanceTest, CreateFlushReopenV4) {
    auto cf = compound_file::create_in_memory(cfb_version::v4);
    ASSERT_TRUE(cf.has_value());

    auto root = cf->root_storage();
    root.create_stream("TestV4");
    std::vector<uint8_t> payload(200, 0xAB);
    root.open_stream("TestV4")->write(0, payload);

    cf->flush();

    auto raw = *cf->data();
    auto cf2 = compound_file::open_from_memory(raw);
    ASSERT_TRUE(cf2.has_value());
    EXPECT_EQ(cf2->version(), cfb_version::v4);

    auto s = cf2->root_storage().open_stream("TestV4");
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->size(), 200u);
    std::vector<uint8_t> buf(200, 0);
    s->read(0, buf);
    EXPECT_EQ(buf, payload);
}

// ── Multiple streams with different sizes ──────────────────────────────

TEST(ConformanceTest, ManyStreams) {
    auto cf = compound_file::create_in_memory(cfb_version::v3);
    ASSERT_TRUE(cf.has_value());
    auto root = cf->root_storage();

    constexpr int count = 10;
    for (int i = 0; i < count; ++i) {
        std::string name = "Stream" + std::to_string(i);
        auto s = root.create_stream(name);
        ASSERT_TRUE(s.has_value());
        std::vector<uint8_t> data(static_cast<size_t>(i + 1) * 10, static_cast<uint8_t>(i));
        s->write(0, data);
    }

    cf->flush();
    auto raw = *cf->data();
    auto cf2 = compound_file::open_from_memory(raw);
    ASSERT_TRUE(cf2.has_value());

    auto root2 = cf2->root_storage();
    for (int i = 0; i < count; ++i) {
        std::string name = "Stream" + std::to_string(i);
        auto s = root2.open_stream(name);
        ASSERT_TRUE(s.has_value()) << "Missing: " << name;
        EXPECT_EQ(s->size(), static_cast<uint64_t>((i + 1) * 10));
        std::vector<uint8_t> buf(s->size(), 0);
        s->read(0, buf);
        for (auto b : buf) EXPECT_EQ(b, static_cast<uint8_t>(i));
    }
}

// ── Nested storages ────────────────────────────────────────────────────

TEST(ConformanceTest, DeepNesting) {
    auto cf = compound_file::create_in_memory(cfb_version::v3);
    ASSERT_TRUE(cf.has_value());

    auto root = cf->root_storage();
    auto level1 = root.create_storage("L1");
    ASSERT_TRUE(level1.has_value());
    auto level2 = level1->create_storage("L2");
    ASSERT_TRUE(level2.has_value());
    auto level3 = level2->create_storage("L3");
    ASSERT_TRUE(level3.has_value());

    auto deep_stream = level3->create_stream("DeepData");
    ASSERT_TRUE(deep_stream.has_value());
    std::vector<uint8_t> data = {1, 2, 3, 4, 5, 6, 7, 8};
    deep_stream->write(0, data);

    cf->flush();
    auto raw = *cf->data();
    auto cf2 = compound_file::open_from_memory(raw);
    ASSERT_TRUE(cf2.has_value());

    auto r = cf2->root_storage();
    auto l1 = r.open_storage("L1");
    ASSERT_TRUE(l1.has_value());
    auto l2 = l1->open_storage("L2");
    ASSERT_TRUE(l2.has_value());
    auto l3 = l2->open_storage("L3");
    ASSERT_TRUE(l3.has_value());
    auto ds = l3->open_stream("DeepData");
    ASSERT_TRUE(ds.has_value());
    EXPECT_EQ(ds->size(), 8u);
    std::vector<uint8_t> buf(8, 0);
    ds->read(0, buf);
    EXPECT_EQ(buf, data);
}

// ── Transaction + conformance ──────────────────────────────────────────

TEST(ConformanceTest, TransactionRevertPreservesFile) {
    auto cf = compound_file::create_in_memory(cfb_version::v3);
    ASSERT_TRUE(cf.has_value());

    auto root = cf->root_storage();
    root.create_stream("Permanent");
    std::vector<uint8_t> pdata = {0xFF};
    root.open_stream("Permanent")->write(0, pdata);
    cf->flush();

    cf->begin_transaction();
    cf->root_storage().create_stream("Temporary");
    cf->root_storage().open_stream("Temporary")->write(0, std::vector<uint8_t>{0x00});
    cf->revert();

    // Verify the file is intact
    auto raw = *cf->data();
    auto cf2 = compound_file::open_from_memory(raw);
    ASSERT_TRUE(cf2.has_value());
    EXPECT_TRUE(cf2->root_storage().exists("Permanent"));
    EXPECT_FALSE(cf2->root_storage().exists("Temporary"));

    auto s = cf2->root_storage().open_stream("Permanent");
    ASSERT_TRUE(s.has_value());
    std::vector<uint8_t> buf(1, 0);
    s->read(0, buf);
    EXPECT_EQ(buf[0], 0xFF);
}

// ── Property set stored in a CFB stream ────────────────────────────────

TEST(ConformanceTest, PropertySetInStream) {
    auto cf = compound_file::create_in_memory(cfb_version::v3);
    ASSERT_TRUE(cf.has_value());

    // Build a SummaryInformation property set
    property_set ps;
    auto& sec = ps.add_section(fmtid_summary_information());
    sec.set_string(pid::title, "Conformance Test");
    sec.set_string(pid::author, "Stout Library");
    sec.set_i4(pid::page_count, 42);

    auto serialized = serialize_property_set(ps);
    ASSERT_TRUE(serialized.has_value());

    // Write it to the standard stream name
    auto root = cf->root_storage();
    auto stream = root.create_stream("\005SummaryInformation");
    ASSERT_TRUE(stream.has_value());
    stream->write(0, *serialized);
    cf->flush();

    // Reopen and read back
    auto raw = *cf->data();
    auto cf2 = compound_file::open_from_memory(raw);
    ASSERT_TRUE(cf2.has_value());

    auto s2 = cf2->root_storage().open_stream("\005SummaryInformation");
    ASSERT_TRUE(s2.has_value());

    std::vector<uint8_t> buf(s2->size(), 0);
    s2->read(0, buf);

    auto ps2 = parse_property_set(buf);
    ASSERT_TRUE(ps2.has_value());
    auto* summary = ps2->section(fmtid_summary_information());
    ASSERT_NE(summary, nullptr);
    EXPECT_EQ(summary->get_string(pid::title), "Conformance Test");
    EXPECT_EQ(summary->get_string(pid::author), "Stout Library");
    EXPECT_EQ(summary->get_i4(pid::page_count), 42);
}

// ── Header validation ──────────────────────────────────────────────────

TEST(ConformanceTest, HeaderSignatureValid) {
    auto cf = compound_file::create_in_memory(cfb_version::v3);
    ASSERT_TRUE(cf.has_value());
    cf->flush();

    auto raw = cf->data();
    ASSERT_NE(raw, nullptr);
    ASSERT_GE(raw->size(), 8u);

    // Check CFB signature
    EXPECT_EQ((*raw)[0], 0xD0);
    EXPECT_EQ((*raw)[1], 0xCF);
    EXPECT_EQ((*raw)[2], 0x11);
    EXPECT_EQ((*raw)[3], 0xE0);
    EXPECT_EQ((*raw)[4], 0xA1);
    EXPECT_EQ((*raw)[5], 0xB1);
    EXPECT_EQ((*raw)[6], 0x1A);
    EXPECT_EQ((*raw)[7], 0xE1);
}

TEST(ConformanceTest, RejectInvalidSignature) {
    std::vector<uint8_t> bad(512, 0);
    auto result = compound_file::open_from_memory(bad);
    ASSERT_FALSE(result.has_value());
}

// ── Empty stream ───────────────────────────────────────────────────────

TEST(ConformanceTest, EmptyStream) {
    auto cf = compound_file::create_in_memory(cfb_version::v3);
    ASSERT_TRUE(cf.has_value());
    auto root = cf->root_storage();
    root.create_stream("Empty");
    cf->flush();

    auto raw = *cf->data();
    auto cf2 = compound_file::open_from_memory(raw);
    ASSERT_TRUE(cf2.has_value());
    auto s = cf2->root_storage().open_stream("Empty");
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->size(), 0u);
}

// ── Remove and re-add ──────────────────────────────────────────────────

TEST(ConformanceTest, RemoveAndReAdd) {
    auto cf = compound_file::create_in_memory(cfb_version::v3);
    ASSERT_TRUE(cf.has_value());
    auto root = cf->root_storage();

    root.create_stream("Reusable");
    root.open_stream("Reusable")->write(0, std::vector<uint8_t>{1, 2, 3});
    root.remove("Reusable");
    EXPECT_FALSE(root.exists("Reusable"));

    root.create_stream("Reusable");
    root.open_stream("Reusable")->write(0, std::vector<uint8_t>{4, 5, 6, 7});

    cf->flush();
    auto raw = *cf->data();
    auto cf2 = compound_file::open_from_memory(raw);
    ASSERT_TRUE(cf2.has_value());

    auto s = cf2->root_storage().open_stream("Reusable");
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->size(), 4u);
    std::vector<uint8_t> buf(4, 0);
    s->read(0, buf);
    std::vector<uint8_t> expected = {4, 5, 6, 7};
    EXPECT_EQ(buf, expected);
}
