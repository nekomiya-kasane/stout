#ifdef _WIN32

#    include "conformance_utils.h"

#    include <gtest/gtest.h>
#    include <stout/compound_file.h>

using namespace conformance;
using namespace stout;

struct VPSector {
    cfb_version ver;
    uint16_t major;
};

class StressSectorBoundaryConformance : public ::testing::TestWithParam<VPSector> {
  protected:
    com_init com_;
    temp_file_guard guard_;
};

static const VPSector vp_sector[] = {{cfb_version::v3, 3}, {cfb_version::v4, 4}};

INSTANTIATE_TEST_SUITE_P(V, StressSectorBoundaryConformance, ::testing::ValuesIn(vp_sector),
                         [](const auto &info) { return info.param.major == 3 ? "V3" : "V4"; });

// ── Exactly 64 bytes (mini-stream sector) ───────────────────────────────

TEST_P(StressSectorBoundaryConformance, Exactly64Bytes) {
    auto p = temp_file("sb_64");
    guard_.add(p);
    auto d = make_test_data(64, 0x11);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("S");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"S", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 64u);
    std::vector<uint8_t> buf(64);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 64, &rc)));
    EXPECT_EQ(buf, d);
}

// ── Exactly 512 bytes (V3 sector size) ──────────────────────────────────

TEST_P(StressSectorBoundaryConformance, Exactly512Bytes) {
    auto p = temp_file("sb_512");
    guard_.add(p);
    auto d = make_test_data(512, 0x22);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("S");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"S", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 512u);
    std::vector<uint8_t> buf(512);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 512, &rc)));
    EXPECT_EQ(buf, d);
}

// ── 511 bytes (one less than V3 sector) ─────────────────────────────────

TEST_P(StressSectorBoundaryConformance, Exactly511Bytes) {
    auto p = temp_file("sb_511");
    guard_.add(p);
    auto d = make_test_data(511, 0x33);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("S");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"S", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 511u);
}

// ── 513 bytes (one more than V3 sector) ─────────────────────────────────

TEST_P(StressSectorBoundaryConformance, Exactly513Bytes) {
    auto p = temp_file("sb_513");
    guard_.add(p);
    auto d = make_test_data(513, 0x44);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("S");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"S", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 513u);
}

// ── 4096 bytes (V4 sector / mini-stream cutoff) ─────────────────────────

TEST_P(StressSectorBoundaryConformance, Exactly4096Bytes) {
    auto p = temp_file("sb_4096");
    guard_.add(p);
    auto d = make_test_data(4096, 0x55);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("S");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"S", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 4096u);
    std::vector<uint8_t> buf(4096);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 4096, &rc)));
    EXPECT_EQ(buf, d);
}

// ── 4095 bytes (one below cutoff) ───────────────────────────────────────

TEST_P(StressSectorBoundaryConformance, Exactly4095Bytes) {
    auto p = temp_file("sb_4095");
    guard_.add(p);
    auto d = make_test_data(4095, 0x66);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("S");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"S", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 4095u);
}

// ── 4097 bytes (one above cutoff) ───────────────────────────────────────

TEST_P(StressSectorBoundaryConformance, Exactly4097Bytes) {
    auto p = temp_file("sb_4097");
    guard_.add(p);
    auto d = make_test_data(4097, 0x77);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("S");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"S", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 4097u);
}

// ── 1024 bytes (two V3 sectors) ─────────────────────────────────────────

TEST_P(StressSectorBoundaryConformance, Exactly1024Bytes) {
    auto p = temp_file("sb_1024");
    guard_.add(p);
    auto d = make_test_data(1024, 0x88);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("S");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"S", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 1024u);
    std::vector<uint8_t> buf(1024);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 1024, &rc)));
    EXPECT_EQ(buf, d);
}

// ── 8192 bytes (two V4 sectors) ─────────────────────────────────────────

TEST_P(StressSectorBoundaryConformance, Exactly8192Bytes) {
    auto p = temp_file("sb_8192");
    guard_.add(p);
    auto d = make_test_data(8192, 0x99);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("S");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"S", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 8192u);
    std::vector<uint8_t> buf(8192);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 8192, &rc)));
    EXPECT_EQ(buf, d);
}

// ── 1 byte stream ───────────────────────────────────────────────────────

TEST_P(StressSectorBoundaryConformance, SingleByte) {
    auto p = temp_file("sb_1");
    guard_.add(p);
    auto d = make_test_data(1, 0xAA);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("S");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"S", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 1u);
}

// ── 0 byte stream ───────────────────────────────────────────────────────

TEST_P(StressSectorBoundaryConformance, ZeroBytes) {
    auto p = temp_file("sb_0");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->root_storage().create_stream("S").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"S", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 0u);
}

// ── Multiple streams at boundary sizes ──────────────────────────────────

TEST_P(StressSectorBoundaryConformance, MultipleBoundarySizes) {
    auto p = temp_file("sb_multi");
    guard_.add(p);
    const uint32_t sizes[] = {0, 1, 63, 64, 65, 511, 512, 513, 4095, 4096, 4097};
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        for (int i = 0; i < 11; ++i) {
            auto s = cf->root_storage().create_stream("S" + std::to_string(i));
            ASSERT_TRUE(s.has_value());
            if (sizes[i] > 0) {
                auto d = make_test_data(sizes[i], static_cast<uint8_t>(i));
                ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
            }
        }
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    for (int i = 0; i < 11; ++i) {
        auto name = L"S" + std::to_wstring(i);
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(stg->OpenStream(name.c_str(), nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
        EXPECT_EQ(win32_stream_size(strm.get()), static_cast<uint64_t>(sizes[i])) << "Stream S" << i;
    }
}

#endif // _WIN32
