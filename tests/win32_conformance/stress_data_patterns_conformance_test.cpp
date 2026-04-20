#ifdef _WIN32

#include "conformance_utils.h"
#include <stout/compound_file.h>
#include <gtest/gtest.h>

using namespace conformance;
using namespace stout;

struct VPData {
    cfb_version ver;
    uint16_t major;
};

class StressDataPatternsConformance : public ::testing::TestWithParam<VPData> {
protected:
    com_init com_;
    temp_file_guard guard_;
};

static const VPData vp_data[] = {{cfb_version::v3, 3}, {cfb_version::v4, 4}};

INSTANTIATE_TEST_SUITE_P(V, StressDataPatternsConformance, ::testing::ValuesIn(vp_data),
    [](const auto& info) { return info.param.major == 3 ? "V3" : "V4"; });

// ── All zeros ───────────────────────────────────────────────────────────

TEST_P(StressDataPatternsConformance, AllZeros100) {
    auto p = temp_file("dp_z100"); guard_.add(p);
    std::vector<uint8_t> d(100, 0x00);
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
    std::vector<uint8_t> buf(100);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 100, &rc)));
    EXPECT_EQ(buf, d);
}

TEST_P(StressDataPatternsConformance, AllZeros1000) {
    auto p = temp_file("dp_z1k"); guard_.add(p);
    std::vector<uint8_t> d(1000, 0x00);
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
    std::vector<uint8_t> buf(1000);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 1000, &rc)));
    EXPECT_EQ(buf, d);
}

// ── All 0xFF ────────────────────────────────────────────────────────────

TEST_P(StressDataPatternsConformance, AllFF100) {
    auto p = temp_file("dp_ff100"); guard_.add(p);
    std::vector<uint8_t> d(100, 0xFF);
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
    std::vector<uint8_t> buf(100);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 100, &rc)));
    EXPECT_EQ(buf, d);
}

TEST_P(StressDataPatternsConformance, AllFF2000) {
    auto p = temp_file("dp_ff2k"); guard_.add(p);
    std::vector<uint8_t> d(2000, 0xFF);
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
    std::vector<uint8_t> buf(2000);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 2000, &rc)));
    EXPECT_EQ(buf, d);
}

// ── Ascending pattern ───────────────────────────────────────────────────

TEST_P(StressDataPatternsConformance, AscendingPattern256) {
    auto p = temp_file("dp_asc256"); guard_.add(p);
    std::vector<uint8_t> d(256);
    for (int i = 0; i < 256; ++i) d[i] = static_cast<uint8_t>(i);
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
    std::vector<uint8_t> buf(256);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 256, &rc)));
    EXPECT_EQ(buf, d);
}

TEST_P(StressDataPatternsConformance, AscendingPattern512) {
    auto p = temp_file("dp_asc512"); guard_.add(p);
    std::vector<uint8_t> d(512);
    for (int i = 0; i < 512; ++i) d[i] = static_cast<uint8_t>(i & 0xFF);
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
    std::vector<uint8_t> buf(512);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 512, &rc)));
    EXPECT_EQ(buf, d);
}

// ── Alternating pattern ─────────────────────────────────────────────────

TEST_P(StressDataPatternsConformance, AlternatingAA55) {
    auto p = temp_file("dp_alt"); guard_.add(p);
    std::vector<uint8_t> d(500);
    for (size_t i = 0; i < 500; ++i) d[i] = (i % 2 == 0) ? 0xAA : 0x55;
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
    std::vector<uint8_t> buf(500);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 500, &rc)));
    EXPECT_EQ(buf, d);
}

// ── Partial read at offset ──────────────────────────────────────────────

TEST_P(StressDataPatternsConformance, PartialReadAtOffset) {
    auto p = temp_file("dp_partial"); guard_.add(p);
    auto d = make_test_data(1000, 0x42);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("S");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto s = cf->root_storage().open_stream("S");
    ASSERT_TRUE(s.has_value());
    std::vector<uint8_t> buf(100);
    ASSERT_TRUE(s->read(500, std::span<uint8_t>(buf)).has_value());
    std::vector<uint8_t> expected(d.begin() + 500, d.begin() + 600);
    EXPECT_EQ(buf, expected);
}

// ── Write at offset ─────────────────────────────────────────────────────

TEST_P(StressDataPatternsConformance, WriteAtOffset) {
    auto p = temp_file("dp_wroff"); guard_.add(p);
    auto d = make_test_data(500, 0x11);
    auto patch = make_test_data(100, 0xFF);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("S");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        ASSERT_TRUE(s->write(200, std::span<const uint8_t>(patch)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto s = cf->root_storage().open_stream("S");
    ASSERT_TRUE(s.has_value());
    std::vector<uint8_t> buf(100);
    ASSERT_TRUE(s->read(200, std::span<uint8_t>(buf)).has_value());
    EXPECT_EQ(buf, patch);
    // Verify untouched region
    std::vector<uint8_t> buf2(100);
    ASSERT_TRUE(s->read(0, std::span<uint8_t>(buf2)).has_value());
    std::vector<uint8_t> expected(d.begin(), d.begin() + 100);
    EXPECT_EQ(buf2, expected);
}

// ── Multiple patterns in same file ──────────────────────────────────────

TEST_P(StressDataPatternsConformance, MultiplePatternsInFile) {
    auto p = temp_file("dp_multi"); guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        for (int i = 0; i < 8; ++i) {
            auto s = cf->root_storage().create_stream("P" + std::to_string(i));
            ASSERT_TRUE(s.has_value());
            auto d = make_test_data(200, static_cast<uint8_t>(i * 0x11));
            ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        }
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    for (int i = 0; i < 8; ++i) {
        auto name = L"P" + std::to_wstring(i);
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(stg->OpenStream(name.c_str(), nullptr,
            STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
        std::vector<uint8_t> buf(200);
        ULONG rc = 0;
        ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 200, &rc)));
        EXPECT_EQ(buf, make_test_data(200, static_cast<uint8_t>(i * 0x11)));
    }
}

// ── Single byte values ──────────────────────────────────────────────────

TEST_P(StressDataPatternsConformance, SingleByte0x00) {
    auto p = temp_file("dp_sb00"); guard_.add(p);
    std::vector<uint8_t> d = {0x00};
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("S");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto s = cf->root_storage().open_stream("S");
    ASSERT_TRUE(s.has_value());
    std::vector<uint8_t> buf(1);
    ASSERT_TRUE(s->read(0, std::span<uint8_t>(buf)).has_value());
    EXPECT_EQ(buf[0], 0x00);
}

TEST_P(StressDataPatternsConformance, SingleByte0xFF) {
    auto p = temp_file("dp_sbFF"); guard_.add(p);
    std::vector<uint8_t> d = {0xFF};
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("S");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto s = cf->root_storage().open_stream("S");
    ASSERT_TRUE(s.has_value());
    std::vector<uint8_t> buf(1);
    ASSERT_TRUE(s->read(0, std::span<uint8_t>(buf)).has_value());
    EXPECT_EQ(buf[0], 0xFF);
}

// ── Repeating 4-byte pattern ────────────────────────────────────────────

TEST_P(StressDataPatternsConformance, Repeating4BytePattern) {
    auto p = temp_file("dp_rep4"); guard_.add(p);
    std::vector<uint8_t> d(400);
    for (size_t i = 0; i < 400; i += 4) {
        d[i] = 0xDE; d[i+1] = 0xAD; d[i+2] = 0xBE; d[i+3] = 0xEF;
    }
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
    std::vector<uint8_t> buf(400);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 400, &rc)));
    EXPECT_EQ(buf, d);
}

// ── Large ascending pattern ─────────────────────────────────────────────

TEST_P(StressDataPatternsConformance, LargeAscending4096) {
    auto p = temp_file("dp_asc4k"); guard_.add(p);
    std::vector<uint8_t> d(4096);
    for (size_t i = 0; i < 4096; ++i) d[i] = static_cast<uint8_t>(i & 0xFF);
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
    std::vector<uint8_t> buf(4096);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 4096, &rc)));
    EXPECT_EQ(buf, d);
}

// ── Win32 writes pattern, Stout reads ───────────────────────────────────

TEST_P(StressDataPatternsConformance, Win32PatternStoutReads) {
    auto p = temp_file("dp_w32pat"); guard_.add(p);
    std::vector<uint8_t> d(300);
    for (size_t i = 0; i < 300; ++i) d[i] = static_cast<uint8_t>((i * 7) & 0xFF);
    {
        storage_ptr stg;
        if (GetParam().ver == cfb_version::v4) ASSERT_TRUE(SUCCEEDED(win32_create_v4(p.wstring(), stg.put())));
        else ASSERT_TRUE(SUCCEEDED(win32_create_v3(p.wstring(), stg.put())));
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(stg->CreateStream(L"S",
            STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, strm.put())));
        ASSERT_TRUE(SUCCEEDED(win32_stream_write(strm.get(), d.data(), 300)));
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto s = cf->root_storage().open_stream("S");
    ASSERT_TRUE(s.has_value());
    std::vector<uint8_t> buf(300);
    ASSERT_TRUE(s->read(0, std::span<uint8_t>(buf)).has_value());
    EXPECT_EQ(buf, d);
}

// ── Stout writes, Win32 reads various sizes ─────────────────────────────

TEST_P(StressDataPatternsConformance, StoutWin32Size10) {
    auto p = temp_file("dp_sw10"); guard_.add(p);
    auto d = make_test_data(10, 0xAB);
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
    std::vector<uint8_t> buf(10);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 10, &rc)));
    EXPECT_EQ(buf, d);
}

TEST_P(StressDataPatternsConformance, StoutWin32Size63) {
    auto p = temp_file("dp_sw63"); guard_.add(p);
    auto d = make_test_data(63, 0xCD);
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
    std::vector<uint8_t> buf(63);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 63, &rc)));
    EXPECT_EQ(buf, d);
}

TEST_P(StressDataPatternsConformance, StoutWin32Size128) {
    auto p = temp_file("dp_sw128"); guard_.add(p);
    auto d = make_test_data(128, 0xEF);
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
    std::vector<uint8_t> buf(128);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 128, &rc)));
    EXPECT_EQ(buf, d);
}

TEST_P(StressDataPatternsConformance, StoutWin32Size2048) {
    auto p = temp_file("dp_sw2k"); guard_.add(p);
    auto d = make_test_data(2048, 0x77);
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
    std::vector<uint8_t> buf(2048);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 2048, &rc)));
    EXPECT_EQ(buf, d);
}

TEST_P(StressDataPatternsConformance, StoutWin32Size3000) {
    auto p = temp_file("dp_sw3k"); guard_.add(p);
    auto d = make_test_data(3000, 0x88);
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
    std::vector<uint8_t> buf(3000);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 3000, &rc)));
    EXPECT_EQ(buf, d);
}

// ── Stout read-back integrity ───────────────────────────────────────────

TEST_P(StressDataPatternsConformance, StoutReadbackIntegrity) {
    auto p = temp_file("dp_readback"); guard_.add(p);
    auto d = make_test_data(750, 0x99);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("S");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto s = cf->root_storage().open_stream("S");
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->size(), 750u);
    std::vector<uint8_t> buf(750);
    ASSERT_TRUE(s->read(0, std::span<uint8_t>(buf)).has_value());
    EXPECT_EQ(buf, d);
}

#endif // _WIN32
