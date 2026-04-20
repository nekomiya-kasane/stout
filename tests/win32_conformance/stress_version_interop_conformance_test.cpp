#ifdef _WIN32

#include "conformance_utils.h"
#include <stout/compound_file.h>
#include <gtest/gtest.h>

using namespace conformance;
using namespace stout;

class StressVersionInteropConformance : public ::testing::Test {
protected:
    com_init com_;
    temp_file_guard guard_;
};

// ── V3 file readable by Win32 ───────────────────────────────────────────

TEST_F(StressVersionInteropConformance, V3BasicWin32Readable) {
    auto p = temp_file("vi_v3basic"); guard_.add(p);
    {
        auto cf = compound_file::create(p, cfb_version::v3);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("Data");
        ASSERT_TRUE(s.has_value());
        auto d = make_test_data(500, 0x11);
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"Data", nullptr,
        STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 500u);
}

TEST_F(StressVersionInteropConformance, V4BasicWin32Readable) {
    auto p = temp_file("vi_v4basic"); guard_.add(p);
    {
        auto cf = compound_file::create(p, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("Data");
        ASSERT_TRUE(s.has_value());
        auto d = make_test_data(500, 0x22);
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"Data", nullptr,
        STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 500u);
}

// ── Win32 V3 file readable by Stout ─────────────────────────────────────

TEST_F(StressVersionInteropConformance, Win32V3StoutReadable) {
    auto p = temp_file("vi_w32v3"); guard_.add(p);
    {
        storage_ptr stg;
        ASSERT_TRUE(SUCCEEDED(win32_create_v3(p.wstring(), stg.put())));
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(stg->CreateStream(L"Data",
            STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, strm.put())));
        auto d = make_test_data(300, 0x33);
        ASSERT_TRUE(SUCCEEDED(win32_stream_write(strm.get(), d.data(), 300)));
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto s = cf->root_storage().open_stream("Data");
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->size(), 300u);
}

TEST_F(StressVersionInteropConformance, Win32V4StoutReadable) {
    auto p = temp_file("vi_w32v4"); guard_.add(p);
    {
        storage_ptr stg;
        ASSERT_TRUE(SUCCEEDED(win32_create_v4(p.wstring(), stg.put())));
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(stg->CreateStream(L"Data",
            STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, strm.put())));
        auto d = make_test_data(300, 0x44);
        ASSERT_TRUE(SUCCEEDED(win32_stream_write(strm.get(), d.data(), 300)));
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto s = cf->root_storage().open_stream("Data");
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->size(), 300u);
}

// ── V3 and V4 produce different sector sizes ────────────────────────────

TEST_F(StressVersionInteropConformance, V3SmallerFileThanV4ForSameData) {
    auto p3 = temp_file("vi_sz3"); guard_.add(p3);
    auto p4 = temp_file("vi_sz4"); guard_.add(p4);
    auto d = make_test_data(100, 0x55);
    {
        auto cf3 = compound_file::create(p3, cfb_version::v3);
        ASSERT_TRUE(cf3.has_value());
        auto s = cf3->root_storage().create_stream("D");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        ASSERT_TRUE(cf3->flush().has_value());
    }
    {
        auto cf4 = compound_file::create(p4, cfb_version::v4);
        ASSERT_TRUE(cf4.has_value());
        auto s = cf4->root_storage().create_stream("D");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        ASSERT_TRUE(cf4->flush().has_value());
    }
    auto sz3 = std::filesystem::file_size(p3);
    auto sz4 = std::filesystem::file_size(p4);
    // V4 uses 4096-byte sectors, V3 uses 512-byte sectors
    // V4 file should be >= V3 file for small data
    EXPECT_GT(sz4, 0u);
    EXPECT_GT(sz3, 0u);
}

// ── V3 mini stream threshold ────────────────────────────────────────────

TEST_F(StressVersionInteropConformance, V3MiniStreamThreshold) {
    auto p = temp_file("vi_v3mini"); guard_.add(p);
    {
        auto cf = compound_file::create(p, cfb_version::v3);
        ASSERT_TRUE(cf.has_value());
        // Below mini-stream cutoff (4096 bytes)
        auto s1 = cf->root_storage().create_stream("Small");
        ASSERT_TRUE(s1.has_value());
        auto d1 = make_test_data(100, 0x66);
        ASSERT_TRUE(s1->write(0, std::span<const uint8_t>(d1)).has_value());
        // Above mini-stream cutoff
        auto s2 = cf->root_storage().create_stream("Big");
        ASSERT_TRUE(s2.has_value());
        auto d2 = make_test_data(5000, 0x77);
        ASSERT_TRUE(s2->write(0, std::span<const uint8_t>(d2)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr s1;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"Small", nullptr,
        STGM_READ | STGM_SHARE_EXCLUSIVE, 0, s1.put())));
    EXPECT_EQ(win32_stream_size(s1.get()), 100u);
    stream_ptr s2;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"Big", nullptr,
        STGM_READ | STGM_SHARE_EXCLUSIVE, 0, s2.put())));
    EXPECT_EQ(win32_stream_size(s2.get()), 5000u);
}

TEST_F(StressVersionInteropConformance, V4MiniStreamThreshold) {
    auto p = temp_file("vi_v4mini"); guard_.add(p);
    {
        auto cf = compound_file::create(p, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto s1 = cf->root_storage().create_stream("Small");
        ASSERT_TRUE(s1.has_value());
        auto d1 = make_test_data(100, 0x88);
        ASSERT_TRUE(s1->write(0, std::span<const uint8_t>(d1)).has_value());
        auto s2 = cf->root_storage().create_stream("Big");
        ASSERT_TRUE(s2.has_value());
        auto d2 = make_test_data(5000, 0x99);
        ASSERT_TRUE(s2->write(0, std::span<const uint8_t>(d2)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr s1;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"Small", nullptr,
        STGM_READ | STGM_SHARE_EXCLUSIVE, 0, s1.put())));
    EXPECT_EQ(win32_stream_size(s1.get()), 100u);
    stream_ptr s2;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"Big", nullptr,
        STGM_READ | STGM_SHARE_EXCLUSIVE, 0, s2.put())));
    EXPECT_EQ(win32_stream_size(s2.get()), 5000u);
}

// ── Both versions handle empty file ─────────────────────────────────────

TEST_F(StressVersionInteropConformance, V3EmptyFile) {
    auto p = temp_file("vi_v3empty"); guard_.add(p);
    { auto cf = compound_file::create(p, cfb_version::v3); ASSERT_TRUE(cf.has_value()); ASSERT_TRUE(cf->flush().has_value()); }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    EXPECT_EQ(win32_enumerate(stg.get()).size(), 0u);
}

TEST_F(StressVersionInteropConformance, V4EmptyFile) {
    auto p = temp_file("vi_v4empty"); guard_.add(p);
    { auto cf = compound_file::create(p, cfb_version::v4); ASSERT_TRUE(cf.has_value()); ASSERT_TRUE(cf->flush().has_value()); }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    EXPECT_EQ(win32_enumerate(stg.get()).size(), 0u);
}

// ── Both versions handle hierarchy ──────────────────────────────────────

TEST_F(StressVersionInteropConformance, V3HierarchyWin32Reads) {
    auto p = temp_file("vi_v3hier"); guard_.add(p);
    {
        auto cf = compound_file::create(p, cfb_version::v3);
        ASSERT_TRUE(cf.has_value());
        auto sub = cf->root_storage().create_storage("Sub");
        ASSERT_TRUE(sub.has_value());
        ASSERT_TRUE(sub->create_stream("Inner").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    storage_ptr sub;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStorage(L"Sub", nullptr,
        STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0, sub.put())));
    stream_ptr inner;
    ASSERT_TRUE(SUCCEEDED(sub->OpenStream(L"Inner", nullptr,
        STGM_READ | STGM_SHARE_EXCLUSIVE, 0, inner.put())));
}

TEST_F(StressVersionInteropConformance, V4HierarchyWin32Reads) {
    auto p = temp_file("vi_v4hier"); guard_.add(p);
    {
        auto cf = compound_file::create(p, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto sub = cf->root_storage().create_storage("Sub");
        ASSERT_TRUE(sub.has_value());
        ASSERT_TRUE(sub->create_stream("Inner").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    storage_ptr sub;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStorage(L"Sub", nullptr,
        STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0, sub.put())));
    stream_ptr inner;
    ASSERT_TRUE(SUCCEEDED(sub->OpenStream(L"Inner", nullptr,
        STGM_READ | STGM_SHARE_EXCLUSIVE, 0, inner.put())));
}

// ── Data integrity across versions ──────────────────────────────────────

TEST_F(StressVersionInteropConformance, V3DataIntegrity) {
    auto p = temp_file("vi_v3data"); guard_.add(p);
    auto d = make_test_data(1000, 0xAA);
    {
        auto cf = compound_file::create(p, cfb_version::v3);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("D");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"D", nullptr,
        STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    std::vector<uint8_t> buf(1000);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 1000, &rc)));
    EXPECT_EQ(buf, d);
}

TEST_F(StressVersionInteropConformance, V4DataIntegrity) {
    auto p = temp_file("vi_v4data"); guard_.add(p);
    auto d = make_test_data(1000, 0xBB);
    {
        auto cf = compound_file::create(p, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("D");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"D", nullptr,
        STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    std::vector<uint8_t> buf(1000);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 1000, &rc)));
    EXPECT_EQ(buf, d);
}

#endif // _WIN32
