#ifdef _WIN32

#    include "conformance_utils.h"

#    include <gtest/gtest.h>
#    include <stout/compound_file.h>

using namespace conformance;
using namespace stout;

class CrossReadConformance : public ::testing::Test {
  protected:
    com_init com_;
    temp_file_guard guard_;
};

// ── Win32 creates complex file, Stout reads everything ──────────────────
TEST_F(CrossReadConformance, Win32CreateStoutRead) {
    auto path = temp_file("xread_w32");
    guard_.add(path);

    auto data1 = make_test_data(300, 0xAA);
    auto data2 = make_test_data(8000, 0xBB);

    {
        storage_ptr stg;
        ASSERT_TRUE(SUCCEEDED(win32_create_v4(path.wstring(), stg.put())));

        // Create sub-storage with a stream
        storage_ptr sub;
        ASSERT_TRUE(SUCCEEDED(
            stg->CreateStorage(L"SubDir", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, sub.put())));
        stream_ptr s1;
        ASSERT_TRUE(SUCCEEDED(
            sub->CreateStream(L"NestedStream", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, s1.put())));
        ASSERT_TRUE(SUCCEEDED(win32_stream_write(s1.get(), data1.data(), 300)));

        // Create root-level stream
        stream_ptr s2;
        ASSERT_TRUE(SUCCEEDED(
            stg->CreateStream(L"BigStream", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, s2.put())));
        ASSERT_TRUE(SUCCEEDED(win32_stream_write(s2.get(), data2.data(), 8000)));
    }

    // Stout reads
    auto cf = compound_file::open(path, open_mode::read);
    ASSERT_TRUE(cf.has_value()) << error_message(cf.error());
    auto root = cf->root_storage();

    // Check root children
    auto children = root.children();
    EXPECT_EQ(children.size(), 2u);

    // Read big stream
    auto bs = root.open_stream("BigStream");
    ASSERT_TRUE(bs.has_value());
    EXPECT_EQ(bs->size(), 8000u);
    std::vector<uint8_t> buf2(8000);
    ASSERT_TRUE(bs->read(0, std::span<uint8_t>(buf2)).has_value());
    EXPECT_EQ(buf2, data2);

    // Read nested stream
    auto sub = root.open_storage("SubDir");
    ASSERT_TRUE(sub.has_value());
    auto ns = sub->open_stream("NestedStream");
    ASSERT_TRUE(ns.has_value());
    EXPECT_EQ(ns->size(), 300u);
    std::vector<uint8_t> buf1(300);
    ASSERT_TRUE(ns->read(0, std::span<uint8_t>(buf1)).has_value());
    EXPECT_EQ(buf1, data1);
}

// ── Stout creates complex file, Win32 reads everything ──────────────────
TEST_F(CrossReadConformance, StoutCreateWin32Read) {
    auto path = temp_file("xread_stout");
    guard_.add(path);

    auto data1 = make_test_data(500, 0xCC);
    auto data2 = make_test_data(6000, 0xDD);

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();

        auto sub = root.create_storage("Folder");
        ASSERT_TRUE(sub.has_value());
        auto s1 = sub->create_stream("Inner");
        ASSERT_TRUE(s1.has_value());
        ASSERT_TRUE(s1->write(0, std::span<const uint8_t>(data1)).has_value());

        auto s2 = root.create_stream("Outer");
        ASSERT_TRUE(s2.has_value());
        ASSERT_TRUE(s2->write(0, std::span<const uint8_t>(data2)).has_value());

        ASSERT_TRUE(cf->flush().has_value());
    }

    // Win32 reads
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));

    // Read outer stream
    stream_ptr s2;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"Outer", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, s2.put())));
    EXPECT_EQ(win32_stream_size(s2.get()), 6000u);
    std::vector<uint8_t> buf2(6000);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(s2.get(), buf2.data(), 6000, &rc)));
    EXPECT_EQ(buf2, data2);

    // Read nested stream
    storage_ptr sub;
    ASSERT_TRUE(
        SUCCEEDED(stg->OpenStorage(L"Folder", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0, sub.put())));
    stream_ptr s1;
    ASSERT_TRUE(SUCCEEDED(sub->OpenStream(L"Inner", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, s1.put())));
    EXPECT_EQ(win32_stream_size(s1.get()), 500u);
    std::vector<uint8_t> buf1(500);
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(s1.get(), buf1.data(), 500, &rc)));
    EXPECT_EQ(buf1, data1);
}

// ── Roundtrip: Stout creates → Win32 modifies → Stout reads ────────────
TEST_F(CrossReadConformance, RoundtripStoutWin32Stout) {
    auto path = temp_file("xread_rt");
    guard_.add(path);

    auto orig_data = make_test_data(200, 0xEE);
    auto added_data = make_test_data(400, 0xFF);

    // Stout creates
    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("Original");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(orig_data)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }

    // Win32 modifies (adds a new stream)
    {
        storage_ptr stg;
        ASSERT_TRUE(SUCCEEDED(win32_open_rw(path.wstring(), stg.put())));
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(
            stg->CreateStream(L"Added", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, strm.put())));
        ASSERT_TRUE(SUCCEEDED(win32_stream_write(strm.get(), added_data.data(), 400)));
    }

    // Stout reads both
    auto cf = compound_file::open(path, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto root = cf->root_storage();
    auto children = root.children();
    EXPECT_EQ(children.size(), 2u);

    auto s1 = root.open_stream("Original");
    ASSERT_TRUE(s1.has_value());
    EXPECT_EQ(s1->size(), 200u);
    std::vector<uint8_t> buf1(200);
    ASSERT_TRUE(s1->read(0, std::span<uint8_t>(buf1)).has_value());
    EXPECT_EQ(buf1, orig_data);

    auto s2 = root.open_stream("Added");
    ASSERT_TRUE(s2.has_value());
    EXPECT_EQ(s2->size(), 400u);
    std::vector<uint8_t> buf2(400);
    ASSERT_TRUE(s2->read(0, std::span<uint8_t>(buf2)).has_value());
    EXPECT_EQ(buf2, added_data);
}

// ── Roundtrip: Win32 creates → Stout modifies → Win32 reads ────────────
TEST_F(CrossReadConformance, RoundtripWin32StoutWin32) {
    auto path = temp_file("xread_rt2");
    guard_.add(path);

    auto orig_data = make_test_data(300, 0x11);
    auto added_data = make_test_data(150, 0x22);

    // Win32 creates
    {
        storage_ptr stg;
        ASSERT_TRUE(SUCCEEDED(win32_create_v4(path.wstring(), stg.put())));
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(
            stg->CreateStream(L"First", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, strm.put())));
        ASSERT_TRUE(SUCCEEDED(win32_stream_write(strm.get(), orig_data.data(), 300)));
    }

    // Stout modifies (adds a stream)
    {
        auto cf = compound_file::open(path, open_mode::read_write);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("Second");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(added_data)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }

    // Win32 reads both
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));

    stream_ptr s1;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"First", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, s1.put())));
    EXPECT_EQ(win32_stream_size(s1.get()), 300u);
    std::vector<uint8_t> buf1(300);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(s1.get(), buf1.data(), 300, &rc)));
    EXPECT_EQ(buf1, orig_data);

    stream_ptr s2;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"Second", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, s2.put())));
    EXPECT_EQ(win32_stream_size(s2.get()), 150u);
    std::vector<uint8_t> buf2(150);
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(s2.get(), buf2.data(), 150, &rc)));
    EXPECT_EQ(buf2, added_data);
}

#endif // _WIN32
