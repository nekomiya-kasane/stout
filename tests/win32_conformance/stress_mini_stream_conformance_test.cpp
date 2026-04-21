#ifdef _WIN32

#include "conformance_utils.h"

#include <gtest/gtest.h>
#include <stout/compound_file.h>

using namespace conformance;
using namespace stout;

struct VersionParam3 {
    cfb_version ver;
    uint16_t major;
};

class StressMiniStreamConformance : public ::testing::TestWithParam<VersionParam3> {
  protected:
    com_init com_;
    temp_file_guard guard_;
};

static const VersionParam3 versions3[] = {
    {cfb_version::v3, 3},
    {cfb_version::v4, 4},
};

INSTANTIATE_TEST_SUITE_P(V, StressMiniStreamConformance, ::testing::ValuesIn(versions3),
                         [](const auto &info) { return info.param.major == 3 ? "V3" : "V4"; });

// Helper: write mini stream with Stout, verify with Win32
static void verify_mini(const std::filesystem::path &path, cfb_version ver, size_t sz, uint8_t seed) {
    auto data = make_test_data(sz, seed);
    {
        auto cf = compound_file::create(path, ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("M");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"M", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), static_cast<uint64_t>(sz));
    std::vector<uint8_t> buf(sz);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), static_cast<ULONG>(sz), &rc)));
    EXPECT_EQ(buf, data);
}

// ── Mini-sector boundary sizes ──────────────────────────────────────────

TEST_P(StressMiniStreamConformance, Size1) {
    auto p = temp_file("ms_1");
    guard_.add(p);
    verify_mini(p, GetParam().ver, 1, 1);
}
TEST_P(StressMiniStreamConformance, Size63) {
    auto p = temp_file("ms_63");
    guard_.add(p);
    verify_mini(p, GetParam().ver, 63, 2);
}
TEST_P(StressMiniStreamConformance, Size64) {
    auto p = temp_file("ms_64");
    guard_.add(p);
    verify_mini(p, GetParam().ver, 64, 3);
}
TEST_P(StressMiniStreamConformance, Size65) {
    auto p = temp_file("ms_65");
    guard_.add(p);
    verify_mini(p, GetParam().ver, 65, 4);
}
TEST_P(StressMiniStreamConformance, Size127) {
    auto p = temp_file("ms_127");
    guard_.add(p);
    verify_mini(p, GetParam().ver, 127, 5);
}
TEST_P(StressMiniStreamConformance, Size128) {
    auto p = temp_file("ms_128");
    guard_.add(p);
    verify_mini(p, GetParam().ver, 128, 6);
}
TEST_P(StressMiniStreamConformance, Size192) {
    auto p = temp_file("ms_192");
    guard_.add(p);
    verify_mini(p, GetParam().ver, 192, 7);
}
TEST_P(StressMiniStreamConformance, Size256) {
    auto p = temp_file("ms_256");
    guard_.add(p);
    verify_mini(p, GetParam().ver, 256, 8);
}
TEST_P(StressMiniStreamConformance, Size512) {
    auto p = temp_file("ms_512");
    guard_.add(p);
    verify_mini(p, GetParam().ver, 512, 9);
}
TEST_P(StressMiniStreamConformance, Size1024) {
    auto p = temp_file("ms_1024");
    guard_.add(p);
    verify_mini(p, GetParam().ver, 1024, 10);
}
TEST_P(StressMiniStreamConformance, Size2048) {
    auto p = temp_file("ms_2048");
    guard_.add(p);
    verify_mini(p, GetParam().ver, 2048, 11);
}
TEST_P(StressMiniStreamConformance, Size3072) {
    auto p = temp_file("ms_3072");
    guard_.add(p);
    verify_mini(p, GetParam().ver, 3072, 12);
}
TEST_P(StressMiniStreamConformance, Size4000) {
    auto p = temp_file("ms_4000");
    guard_.add(p);
    verify_mini(p, GetParam().ver, 4000, 13);
}
TEST_P(StressMiniStreamConformance, Size4095) {
    auto p = temp_file("ms_4095");
    guard_.add(p);
    verify_mini(p, GetParam().ver, 4095, 14);
}

// ── Multiple mini streams ───────────────────────────────────────────────

TEST_P(StressMiniStreamConformance, FiftyMiniStreams64Bytes) {
    auto p = temp_file("ms_50x64");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        for (int i = 0; i < 50; ++i) {
            auto s = root.create_stream("M" + std::to_string(i));
            ASSERT_TRUE(s.has_value());
            auto data = make_test_data(64, static_cast<uint8_t>(i));
            ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        }
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    for (int i = 0; i < 50; ++i) {
        auto name = L"M" + std::to_wstring(i);
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(stg->OpenStream(name.c_str(), nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
        EXPECT_EQ(win32_stream_size(strm.get()), 64u);
        std::vector<uint8_t> buf(64);
        ULONG rc = 0;
        ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 64, &rc)));
        EXPECT_EQ(buf, make_test_data(64, static_cast<uint8_t>(i))) << "Stream M" << i;
    }
}

TEST_P(StressMiniStreamConformance, SixMiniVaryingSizes) {
    auto p = temp_file("ms_6vary");
    guard_.add(p);
    size_t sizes[] = {10, 50, 100, 200, 500, 1000};
    constexpr int count = 6;
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        for (int i = 0; i < count; ++i) {
            auto s = root.create_stream("V" + std::to_string(i));
            ASSERT_TRUE(s.has_value());
            auto data = make_test_data(sizes[i], static_cast<uint8_t>(i + 50));
            ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        }
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    for (int i = 0; i < count; ++i) {
        auto name = L"V" + std::to_wstring(i);
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(stg->OpenStream(name.c_str(), nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
        EXPECT_EQ(win32_stream_size(strm.get()), static_cast<uint64_t>(sizes[i]));
        std::vector<uint8_t> buf(sizes[i]);
        ULONG rc = 0;
        ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), static_cast<ULONG>(sizes[i]), &rc)));
        EXPECT_EQ(buf, make_test_data(sizes[i], static_cast<uint8_t>(i + 50))) << "Stream V" << i;
    }
}

// ── Mini stream after delete ────────────────────────────────────────────

TEST_P(StressMiniStreamConformance, MiniAfterDelete) {
    auto p = temp_file("ms_del");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        auto s1 = root.create_stream("A");
        ASSERT_TRUE(s1.has_value());
        auto d1 = make_test_data(200, 0x11);
        ASSERT_TRUE(s1->write(0, std::span<const uint8_t>(d1)).has_value());
        auto s2 = root.create_stream("B");
        ASSERT_TRUE(s2.has_value());
        auto d2 = make_test_data(300, 0x22);
        ASSERT_TRUE(s2->write(0, std::span<const uint8_t>(d2)).has_value());
        ASSERT_TRUE(root.remove("A").has_value());
        // Create new mini stream
        auto s3 = root.create_stream("C");
        ASSERT_TRUE(s3.has_value());
        auto d3 = make_test_data(150, 0x33);
        ASSERT_TRUE(s3->write(0, std::span<const uint8_t>(d3)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    auto entries = win32_enumerate(stg.get());
    EXPECT_EQ(entries.size(), 2u);
    for (auto &e : entries) {
        free_statstg_name(e);
    }
    // Verify B data
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"B", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 300u);
    std::vector<uint8_t> buf(300);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 300, &rc)));
    EXPECT_EQ(buf, make_test_data(300, 0x22));
}

// ── Mini stream integrity after adding regular stream ───────────────────

TEST_P(StressMiniStreamConformance, MiniIntegrityAfterRegular) {
    auto p = temp_file("ms_reg");
    guard_.add(p);
    auto mini_data = make_test_data(500, 0xAA);
    auto reg_data = make_test_data(8000, 0xBB);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        auto s1 = root.create_stream("Mini");
        ASSERT_TRUE(s1.has_value());
        ASSERT_TRUE(s1->write(0, std::span<const uint8_t>(mini_data)).has_value());
        auto s2 = root.create_stream("Regular");
        ASSERT_TRUE(s2.has_value());
        ASSERT_TRUE(s2->write(0, std::span<const uint8_t>(reg_data)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    // Verify mini
    stream_ptr ms;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"Mini", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, ms.put())));
    EXPECT_EQ(win32_stream_size(ms.get()), 500u);
    std::vector<uint8_t> mbuf(500);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(ms.get(), mbuf.data(), 500, &rc)));
    EXPECT_EQ(mbuf, mini_data);
    // Verify regular
    stream_ptr rs;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"Regular", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, rs.put())));
    EXPECT_EQ(win32_stream_size(rs.get()), 8000u);
}

// ── Win32 writes mini, Stout reads ──────────────────────────────────────

TEST_P(StressMiniStreamConformance, Win32MiniStoutRead100) {
    auto p = temp_file("ms_w100");
    guard_.add(p);
    auto data = make_test_data(100, 0x77);
    {
        storage_ptr stg;
        if (GetParam().ver == cfb_version::v4) {
            ASSERT_TRUE(SUCCEEDED(win32_create_v4(p.wstring(), stg.put())));
        } else {
            ASSERT_TRUE(SUCCEEDED(win32_create_v3(p.wstring(), stg.put())));
        }
        stream_ptr strm;
        ASSERT_TRUE(
            SUCCEEDED(stg->CreateStream(L"W", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, strm.put())));
        ASSERT_TRUE(SUCCEEDED(win32_stream_write(strm.get(), data.data(), 100)));
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto s = cf->root_storage().open_stream("W");
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->size(), 100u);
    std::vector<uint8_t> buf(100);
    ASSERT_TRUE(s->read(0, std::span<uint8_t>(buf)).has_value());
    EXPECT_EQ(buf, data);
}

TEST_P(StressMiniStreamConformance, Win32MiniStoutRead2000) {
    auto p = temp_file("ms_w2k");
    guard_.add(p);
    auto data = make_test_data(2000, 0x88);
    {
        storage_ptr stg;
        if (GetParam().ver == cfb_version::v4) {
            ASSERT_TRUE(SUCCEEDED(win32_create_v4(p.wstring(), stg.put())));
        } else {
            ASSERT_TRUE(SUCCEEDED(win32_create_v3(p.wstring(), stg.put())));
        }
        stream_ptr strm;
        ASSERT_TRUE(
            SUCCEEDED(stg->CreateStream(L"W", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, strm.put())));
        ASSERT_TRUE(SUCCEEDED(win32_stream_write(strm.get(), data.data(), 2000)));
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto s = cf->root_storage().open_stream("W");
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->size(), 2000u);
    std::vector<uint8_t> buf(2000);
    ASSERT_TRUE(s->read(0, std::span<uint8_t>(buf)).has_value());
    EXPECT_EQ(buf, data);
}

// ── Mini stream in sub-storage ──────────────────────────────────────────

TEST_P(StressMiniStreamConformance, MiniInSubStorage) {
    auto p = temp_file("ms_sub");
    guard_.add(p);
    auto data = make_test_data(300, 0xCC);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto sub = cf->root_storage().create_storage("Sub");
        ASSERT_TRUE(sub.has_value());
        auto s = sub->create_stream("Inner");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    storage_ptr sub;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStorage(L"Sub", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0, sub.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(sub->OpenStream(L"Inner", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 300u);
    std::vector<uint8_t> buf(300);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 300, &rc)));
    EXPECT_EQ(buf, data);
}

// ── Mini stream partial read ────────────────────────────────────────────

TEST_P(StressMiniStreamConformance, MiniPartialRead) {
    auto p = temp_file("ms_partial");
    guard_.add(p);
    auto data = make_test_data(500, 0xDD);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("P");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto s = cf->root_storage().open_stream("P");
    ASSERT_TRUE(s.has_value());
    // Read 100 bytes from offset 200
    std::vector<uint8_t> buf(100);
    auto rd = s->read(200, std::span<uint8_t>(buf));
    ASSERT_TRUE(rd.has_value());
    EXPECT_EQ(*rd, 100u);
    EXPECT_EQ(buf, std::vector<uint8_t>(data.begin() + 200, data.begin() + 300));
}

// ── Mixed mini and regular with data verification ───────────────────────

TEST_P(StressMiniStreamConformance, FiveMiniPlusFiveRegular) {
    auto p = temp_file("ms_5x5");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        for (int i = 0; i < 5; ++i) {
            auto sm = root.create_stream("Mini" + std::to_string(i));
            ASSERT_TRUE(sm.has_value());
            auto dm = make_test_data(200 + i * 100, static_cast<uint8_t>(i));
            ASSERT_TRUE(sm->write(0, std::span<const uint8_t>(dm)).has_value());
            auto sr = root.create_stream("Reg" + std::to_string(i));
            ASSERT_TRUE(sr.has_value());
            auto dr = make_test_data(5000 + i * 1000, static_cast<uint8_t>(i + 100));
            ASSERT_TRUE(sr->write(0, std::span<const uint8_t>(dr)).has_value());
        }
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    for (int i = 0; i < 5; ++i) {
        auto mname = L"Mini" + std::to_wstring(i);
        stream_ptr ms;
        ASSERT_TRUE(SUCCEEDED(stg->OpenStream(mname.c_str(), nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, ms.put())));
        auto expected_sz = 200 + i * 100;
        EXPECT_EQ(win32_stream_size(ms.get()), static_cast<uint64_t>(expected_sz));
        std::vector<uint8_t> buf(expected_sz);
        ULONG rc = 0;
        ASSERT_TRUE(SUCCEEDED(win32_stream_read(ms.get(), buf.data(), static_cast<ULONG>(expected_sz), &rc)));
        EXPECT_EQ(buf, make_test_data(expected_sz, static_cast<uint8_t>(i)));

        auto rname = L"Reg" + std::to_wstring(i);
        stream_ptr rs;
        ASSERT_TRUE(SUCCEEDED(stg->OpenStream(rname.c_str(), nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, rs.put())));
        auto reg_sz = 5000 + i * 1000;
        EXPECT_EQ(win32_stream_size(rs.get()), static_cast<uint64_t>(reg_sz));
    }
}

#endif // _WIN32
