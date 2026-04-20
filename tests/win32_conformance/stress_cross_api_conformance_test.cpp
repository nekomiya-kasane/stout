#ifdef _WIN32

#include "conformance_utils.h"

#include <gtest/gtest.h>
#include <stout/compound_file.h>

using namespace conformance;
using namespace stout;

struct VPCross {
    cfb_version ver;
    uint16_t major;
};

class StressCrossAPIConformance : public ::testing::TestWithParam<VPCross> {
  protected:
    com_init com_;
    temp_file_guard guard_;
};

static const VPCross vp_cross[] = {{cfb_version::v3, 3}, {cfb_version::v4, 4}};

INSTANTIATE_TEST_SUITE_P(V, StressCrossAPIConformance, ::testing::ValuesIn(vp_cross),
                         [](const auto &info) { return info.param.major == 3 ? "V3" : "V4"; });

// ── Win32 creates complex structure, Stout reads everything ─────────────

TEST_P(StressCrossAPIConformance, Win32ComplexStoutReads) {
    auto p = temp_file("sx_w2s");
    guard_.add(p);
    {
        storage_ptr stg;
        if (GetParam().ver == cfb_version::v4)
            ASSERT_TRUE(SUCCEEDED(win32_create_v4(p.wstring(), stg.put())));
        else
            ASSERT_TRUE(SUCCEEDED(win32_create_v3(p.wstring(), stg.put())));
        // Create streams
        for (int i = 0; i < 5; ++i) {
            auto name = L"Stream" + std::to_wstring(i);
            stream_ptr strm;
            ASSERT_TRUE(SUCCEEDED(stg->CreateStream(name.c_str(), STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE,
                                                    0, 0, strm.put())));
            auto data = make_test_data(100 * (i + 1), static_cast<uint8_t>(i));
            ASSERT_TRUE(SUCCEEDED(win32_stream_write(strm.get(), data.data(), static_cast<ULONG>(data.size()))));
        }
        // Create sub-storage with streams
        storage_ptr sub;
        ASSERT_TRUE(SUCCEEDED(
            stg->CreateStorage(L"SubDir", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, sub.put())));
        stream_ptr substrm;
        ASSERT_TRUE(SUCCEEDED(
            sub->CreateStream(L"SubStream", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, substrm.put())));
        auto sd = make_test_data(250, 0xAA);
        ASSERT_TRUE(SUCCEEDED(win32_stream_write(substrm.get(), sd.data(), 250)));
    }
    // Stout reads
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto root = cf->root_storage();
    auto kids = root.children();
    EXPECT_EQ(kids.size(), 6u); // 5 streams + 1 storage
    for (int i = 0; i < 5; ++i) {
        auto s = root.open_stream("Stream" + std::to_string(i));
        ASSERT_TRUE(s.has_value()) << "Stream" << i;
        EXPECT_EQ(s->size(), static_cast<uint64_t>(100 * (i + 1)));
        std::vector<uint8_t> buf(s->size());
        ASSERT_TRUE(s->read(0, std::span<uint8_t>(buf)).has_value());
        EXPECT_EQ(buf, make_test_data(100 * (i + 1), static_cast<uint8_t>(i)));
    }
    auto sub = root.open_storage("SubDir");
    ASSERT_TRUE(sub.has_value());
    auto ss = sub->open_stream("SubStream");
    ASSERT_TRUE(ss.has_value());
    EXPECT_EQ(ss->size(), 250u);
}

// ── Stout creates complex structure, Win32 reads everything ─────────────

TEST_P(StressCrossAPIConformance, StoutComplexWin32Reads) {
    auto p = temp_file("sx_s2w");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        for (int i = 0; i < 5; ++i) {
            auto s = root.create_stream("S" + std::to_string(i));
            ASSERT_TRUE(s.has_value());
            auto data = make_test_data(200 * (i + 1), static_cast<uint8_t>(i + 10));
            ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        }
        auto sub = root.create_storage("Dir");
        ASSERT_TRUE(sub.has_value());
        auto ss = sub->create_stream("Inner");
        ASSERT_TRUE(ss.has_value());
        auto d = make_test_data(500, 0xBB);
        ASSERT_TRUE(ss->write(0, std::span<const uint8_t>(d)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    auto entries = win32_enumerate(stg.get());
    EXPECT_EQ(entries.size(), 6u);
    for (auto &e : entries) free_statstg_name(e);
    for (int i = 0; i < 5; ++i) {
        auto name = L"S" + std::to_wstring(i);
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(stg->OpenStream(name.c_str(), nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
        EXPECT_EQ(win32_stream_size(strm.get()), static_cast<uint64_t>(200 * (i + 1)));
    }
    storage_ptr sub;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStorage(L"Dir", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0, sub.put())));
    stream_ptr inner;
    ASSERT_TRUE(SUCCEEDED(sub->OpenStream(L"Inner", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, inner.put())));
    EXPECT_EQ(win32_stream_size(inner.get()), 500u);
}

// ── Roundtrip: Stout creates, Win32 modifies, Stout reads ──────────────

TEST_P(StressCrossAPIConformance, RoundtripModify) {
    auto p = temp_file("sx_rt");
    guard_.add(p);
    auto orig = make_test_data(300, 0x11);
    auto modified = make_test_data(500, 0x22);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("Data");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(orig)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    // Win32 modifies
    {
        storage_ptr stg;
        ASSERT_TRUE(SUCCEEDED(win32_open_rw(p.wstring(), stg.put())));
        stg->DestroyElement(L"Data");
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(
            stg->CreateStream(L"Data", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, strm.put())));
        ASSERT_TRUE(SUCCEEDED(win32_stream_write(strm.get(), modified.data(), 500)));
    }
    // Stout reads
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto s = cf->root_storage().open_stream("Data");
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->size(), 500u);
    std::vector<uint8_t> buf(500);
    ASSERT_TRUE(s->read(0, std::span<uint8_t>(buf)).has_value());
    EXPECT_EQ(buf, modified);
}

// ── Win32 creates, Stout modifies, Win32 reads ─────────────────────────

TEST_P(StressCrossAPIConformance, RoundtripStoutModifies) {
    auto p = temp_file("sx_rt2");
    guard_.add(p);
    auto orig = make_test_data(200, 0x33);
    auto added = make_test_data(400, 0x44);
    {
        storage_ptr stg;
        if (GetParam().ver == cfb_version::v4)
            ASSERT_TRUE(SUCCEEDED(win32_create_v4(p.wstring(), stg.put())));
        else
            ASSERT_TRUE(SUCCEEDED(win32_create_v3(p.wstring(), stg.put())));
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(
            stg->CreateStream(L"Original", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, strm.put())));
        ASSERT_TRUE(SUCCEEDED(win32_stream_write(strm.get(), orig.data(), 200)));
    }
    // Stout modifies
    {
        auto cf = compound_file::open(p, open_mode::read_write);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("Added");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(added)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    // Win32 reads
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    auto entries = win32_enumerate(stg.get());
    EXPECT_EQ(entries.size(), 2u);
    for (auto &e : entries) free_statstg_name(e);
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"Added", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 400u);
    std::vector<uint8_t> buf(400);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 400, &rc)));
    EXPECT_EQ(buf, added);
}

// ── Win32 creates deep hierarchy, Stout traverses ───────────────────────

TEST_P(StressCrossAPIConformance, Win32DeepHierarchyStoutTraverses) {
    auto p = temp_file("sx_deep");
    guard_.add(p);
    {
        storage_ptr stg;
        if (GetParam().ver == cfb_version::v4)
            ASSERT_TRUE(SUCCEEDED(win32_create_v4(p.wstring(), stg.put())));
        else
            ASSERT_TRUE(SUCCEEDED(win32_create_v3(p.wstring(), stg.put())));
        IStorage *cur = stg.get();
        storage_ptr levels[5];
        for (int i = 0; i < 5; ++i) {
            auto name = L"Level" + std::to_wstring(i);
            ASSERT_TRUE(SUCCEEDED(cur->CreateStorage(name.c_str(), STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE,
                                                     0, 0, levels[i].put())));
            cur = levels[i].get();
        }
        stream_ptr leaf;
        ASSERT_TRUE(SUCCEEDED(
            cur->CreateStream(L"Leaf", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, leaf.put())));
        auto d = make_test_data(50, 0xCC);
        ASSERT_TRUE(SUCCEEDED(win32_stream_write(leaf.get(), d.data(), 50)));
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto s0 = cf->root_storage().open_storage("Level0");
    ASSERT_TRUE(s0.has_value());
    auto s1 = s0->open_storage("Level1");
    ASSERT_TRUE(s1.has_value());
    auto s2 = s1->open_storage("Level2");
    ASSERT_TRUE(s2.has_value());
    auto s3 = s2->open_storage("Level3");
    ASSERT_TRUE(s3.has_value());
    auto s4 = s3->open_storage("Level4");
    ASSERT_TRUE(s4.has_value());
    auto leaf = s4->open_stream("Leaf");
    ASSERT_TRUE(leaf.has_value());
    EXPECT_EQ(leaf->size(), 50u);
}

// ── Mixed mini and regular streams cross-API ────────────────────────────

TEST_P(StressCrossAPIConformance, MixedMiniRegularCrossAPI) {
    auto p = temp_file("sx_mixed");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        // Mini stream
        auto s1 = root.create_stream("Mini");
        ASSERT_TRUE(s1.has_value());
        auto d1 = make_test_data(100, 0x11);
        ASSERT_TRUE(s1->write(0, std::span<const uint8_t>(d1)).has_value());
        // Regular stream
        auto s2 = root.create_stream("Regular");
        ASSERT_TRUE(s2.has_value());
        auto d2 = make_test_data(5000, 0x22);
        ASSERT_TRUE(s2->write(0, std::span<const uint8_t>(d2)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    // Read mini
    stream_ptr s1;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"Mini", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, s1.put())));
    EXPECT_EQ(win32_stream_size(s1.get()), 100u);
    std::vector<uint8_t> buf1(100);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(s1.get(), buf1.data(), 100, &rc)));
    EXPECT_EQ(buf1, make_test_data(100, 0x11));
    // Read regular
    stream_ptr s2;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"Regular", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, s2.put())));
    EXPECT_EQ(win32_stream_size(s2.get()), 5000u);
    std::vector<uint8_t> buf2(5000);
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(s2.get(), buf2.data(), 5000, &rc)));
    EXPECT_EQ(buf2, make_test_data(5000, 0x22));
}

// ── Empty file cross-API ────────────────────────────────────────────────

TEST_P(StressCrossAPIConformance, EmptyFileCrossAPI) {
    auto p = temp_file("sx_empty");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    EXPECT_EQ(win32_enumerate(stg.get()).size(), 0u);
}

TEST_P(StressCrossAPIConformance, Win32EmptyStoutReads) {
    auto p = temp_file("sx_w32empty");
    guard_.add(p);
    {
        storage_ptr stg;
        if (GetParam().ver == cfb_version::v4)
            ASSERT_TRUE(SUCCEEDED(win32_create_v4(p.wstring(), stg.put())));
        else
            ASSERT_TRUE(SUCCEEDED(win32_create_v3(p.wstring(), stg.put())));
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    EXPECT_EQ(cf->root_storage().children().size(), 0u);
}

#endif // _WIN32
