#ifdef _WIN32

#    include "conformance_utils.h"

#    include <gtest/gtest.h>
#    include <stout/compound_file.h>

using namespace conformance;
using namespace stout;

struct VPRound {
    cfb_version ver;
    uint16_t major;
};

class StressRoundtripConformance : public ::testing::TestWithParam<VPRound> {
  protected:
    com_init com_;
    temp_file_guard guard_;
};

static const VPRound vp_round[] = {{cfb_version::v3, 3}, {cfb_version::v4, 4}};

INSTANTIATE_TEST_SUITE_P(V, StressRoundtripConformance, ::testing::ValuesIn(vp_round),
                         [](const auto &info) { return info.param.major == 3 ? "V3" : "V4"; });

// ── Stout create → Win32 read → Stout read ──────────────────────────────

TEST_P(StressRoundtripConformance, StoutWin32StoutBasic) {
    auto p = temp_file("rt_sws");
    guard_.add(p);
    auto d = make_test_data(800, 0x11);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("Data");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    // Win32 verify
    {
        storage_ptr stg;
        ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"Data", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
        EXPECT_EQ(win32_stream_size(strm.get()), 800u);
        std::vector<uint8_t> buf(800);
        ULONG rc = 0;
        ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 800, &rc)));
        EXPECT_EQ(buf, d);
    }
    // Stout re-read
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto s = cf->root_storage().open_stream("Data");
    ASSERT_TRUE(s.has_value());
    std::vector<uint8_t> buf(800);
    ASSERT_TRUE(s->read(0, std::span<uint8_t>(buf)).has_value());
    EXPECT_EQ(buf, d);
}

// ── Win32 create → Stout read → Win32 read ──────────────────────────────

TEST_P(StressRoundtripConformance, Win32StoutWin32Basic) {
    auto p = temp_file("rt_wsw");
    guard_.add(p);
    auto d = make_test_data(600, 0x22);
    {
        storage_ptr stg;
        if (GetParam().ver == cfb_version::v4) {
            ASSERT_TRUE(SUCCEEDED(win32_create_v4(p.wstring(), stg.put())));
        } else {
            ASSERT_TRUE(SUCCEEDED(win32_create_v3(p.wstring(), stg.put())));
        }
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(
            stg->CreateStream(L"Data", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, strm.put())));
        ASSERT_TRUE(SUCCEEDED(win32_stream_write(strm.get(), d.data(), 600)));
    }
    // Stout verify
    {
        auto cf = compound_file::open(p, open_mode::read);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().open_stream("Data");
        ASSERT_TRUE(s.has_value());
        EXPECT_EQ(s->size(), 600u);
        std::vector<uint8_t> buf(600);
        ASSERT_TRUE(s->read(0, std::span<uint8_t>(buf)).has_value());
        EXPECT_EQ(buf, d);
    }
    // Win32 re-read
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"Data", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    std::vector<uint8_t> buf(600);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 600, &rc)));
    EXPECT_EQ(buf, d);
}

// ── Stout create → Stout modify → Win32 read ───────────────────────────

TEST_P(StressRoundtripConformance, StoutModifyWin32Read) {
    auto p = temp_file("rt_smw");
    guard_.add(p);
    auto d1 = make_test_data(400, 0x33);
    auto d2 = make_test_data(600, 0x44);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("Data");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d1)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    {
        auto cf = compound_file::open(p, open_mode::read_write);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().open_stream("Data");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d2)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"Data", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 600u);
    std::vector<uint8_t> buf(600);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 600, &rc)));
    EXPECT_EQ(buf, d2);
}

// ── Win32 create → Win32 modify → Stout read ───────────────────────────

TEST_P(StressRoundtripConformance, Win32ModifyStoutRead) {
    auto p = temp_file("rt_wms");
    guard_.add(p);
    auto d1 = make_test_data(300, 0x55);
    auto d2 = make_test_data(700, 0x66);
    {
        storage_ptr stg;
        if (GetParam().ver == cfb_version::v4) {
            ASSERT_TRUE(SUCCEEDED(win32_create_v4(p.wstring(), stg.put())));
        } else {
            ASSERT_TRUE(SUCCEEDED(win32_create_v3(p.wstring(), stg.put())));
        }
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(
            stg->CreateStream(L"Data", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, strm.put())));
        ASSERT_TRUE(SUCCEEDED(win32_stream_write(strm.get(), d1.data(), 300)));
    }
    {
        storage_ptr stg;
        ASSERT_TRUE(SUCCEEDED(win32_open_rw(p.wstring(), stg.put())));
        stg->DestroyElement(L"Data");
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(
            stg->CreateStream(L"Data", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, strm.put())));
        ASSERT_TRUE(SUCCEEDED(win32_stream_write(strm.get(), d2.data(), 700)));
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto s = cf->root_storage().open_stream("Data");
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->size(), 700u);
    std::vector<uint8_t> buf(700);
    ASSERT_TRUE(s->read(0, std::span<uint8_t>(buf)).has_value());
    EXPECT_EQ(buf, d2);
}

// ── Hierarchy roundtrip ─────────────────────────────────────────────────

TEST_P(StressRoundtripConformance, HierarchyRoundtrip) {
    auto p = temp_file("rt_hier");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto sub = cf->root_storage().create_storage("Sub");
        ASSERT_TRUE(sub.has_value());
        auto s = sub->create_stream("Inner");
        ASSERT_TRUE(s.has_value());
        auto d = make_test_data(400, 0x77);
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    // Win32 verify hierarchy
    {
        storage_ptr stg;
        ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
        storage_ptr sub;
        ASSERT_TRUE(
            SUCCEEDED(stg->OpenStorage(L"Sub", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0, sub.put())));
        stream_ptr inner;
        ASSERT_TRUE(SUCCEEDED(sub->OpenStream(L"Inner", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, inner.put())));
        EXPECT_EQ(win32_stream_size(inner.get()), 400u);
    }
    // Stout re-read
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto sub = cf->root_storage().open_storage("Sub");
    ASSERT_TRUE(sub.has_value());
    auto s = sub->open_stream("Inner");
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->size(), 400u);
}

// ── Mini stream roundtrip ───────────────────────────────────────────────

TEST_P(StressRoundtripConformance, MiniStreamRoundtrip) {
    auto p = temp_file("rt_mini");
    guard_.add(p);
    auto d = make_test_data(100, 0x88);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("Mini");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"Mini", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    std::vector<uint8_t> buf(100);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 100, &rc)));
    EXPECT_EQ(buf, d);
}

// ── Multiple streams roundtrip ──────────────────────────────────────────

TEST_P(StressRoundtripConformance, MultiStreamRoundtrip) {
    auto p = temp_file("rt_multi");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        for (int i = 0; i < 10; ++i) {
            auto s = cf->root_storage().create_stream("S" + std::to_string(i));
            ASSERT_TRUE(s.has_value());
            auto d = make_test_data(100 * (i + 1), static_cast<uint8_t>(i));
            ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        }
        ASSERT_TRUE(cf->flush().has_value());
    }
    // Win32 verify
    {
        storage_ptr stg;
        ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
        for (int i = 0; i < 10; ++i) {
            auto name = L"S" + std::to_wstring(i);
            stream_ptr strm;
            ASSERT_TRUE(
                SUCCEEDED(stg->OpenStream(name.c_str(), nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
            EXPECT_EQ(win32_stream_size(strm.get()), static_cast<uint64_t>(100 * (i + 1)));
        }
    }
    // Stout re-read
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    for (int i = 0; i < 10; ++i) {
        auto s = cf->root_storage().open_stream("S" + std::to_string(i));
        ASSERT_TRUE(s.has_value());
        EXPECT_EQ(s->size(), static_cast<uint64_t>(100 * (i + 1)));
    }
}

// ── Add stream after reopen ─────────────────────────────────────────────

TEST_P(StressRoundtripConformance, AddStreamAfterReopen) {
    auto p = temp_file("rt_addreopen");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("First");
        ASSERT_TRUE(s.has_value());
        auto d = make_test_data(200, 0x99);
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    {
        auto cf = compound_file::open(p, open_mode::read_write);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("Second");
        ASSERT_TRUE(s.has_value());
        auto d = make_test_data(300, 0xAA);
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    auto entries = win32_enumerate(stg.get());
    EXPECT_EQ(entries.size(), 2u);
    for (auto &e : entries) {
        free_statstg_name(e);
    }
}

// ── Delete and re-add roundtrip ─────────────────────────────────────────

TEST_P(StressRoundtripConformance, DeleteReaddRoundtrip) {
    auto p = temp_file("rt_delreadd");
    guard_.add(p);
    auto d1 = make_test_data(200, 0xBB);
    auto d2 = make_test_data(400, 0xCC);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("X");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d1)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    {
        auto cf = compound_file::open(p, open_mode::read_write);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->root_storage().remove("X").has_value());
        auto s = cf->root_storage().create_stream("X");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d2)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"X", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 400u);
    std::vector<uint8_t> buf(400);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 400, &rc)));
    EXPECT_EQ(buf, d2);
}

#endif // _WIN32
