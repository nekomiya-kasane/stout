#ifdef _WIN32

#include "conformance_utils.h"
#include <stout/compound_file.h>
#include <gtest/gtest.h>
#include <algorithm>
#include <numeric>

using namespace conformance;
using namespace stout;

struct VersionParam2 {
    cfb_version ver;
    uint16_t major;
    uint32_t sector_size;
};

class StressStreamIOConformance : public ::testing::TestWithParam<VersionParam2> {
protected:
    com_init com_;
    temp_file_guard guard_;
};

static const VersionParam2 versions2[] = {
    {cfb_version::v3, 3, 512},
    {cfb_version::v4, 4, 4096},
};

INSTANTIATE_TEST_SUITE_P(V, StressStreamIOConformance, ::testing::ValuesIn(versions2),
    [](const auto& info) { return info.param.major == 3 ? "V3" : "V4"; });

// Helper: Stout writes N bytes, Win32 reads and verifies
static void stout_write_win32_read(const std::filesystem::path& path, cfb_version ver,
                                     const std::string& stream_name, size_t sz, uint8_t seed) {
    auto data = make_test_data(sz, seed);
    {
        auto cf = compound_file::create(path, ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream(stream_name);
        ASSERT_TRUE(s.has_value());
        if (sz > 0) ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    auto wname = std::wstring(stream_name.begin(), stream_name.end());
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(wname.c_str(), nullptr,
        STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), static_cast<uint64_t>(sz));
    if (sz > 0) {
        std::vector<uint8_t> buf(sz);
        ULONG rc = 0;
        ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), static_cast<ULONG>(sz), &rc)));
        EXPECT_EQ(rc, static_cast<ULONG>(sz));
        EXPECT_EQ(buf, data);
    }
}

// Helper: Win32 writes N bytes, Stout reads and verifies
static void win32_write_stout_read(const std::filesystem::path& path, cfb_version ver,
                                     const std::string& stream_name, size_t sz, uint8_t seed) {
    auto data = make_test_data(sz, seed);
    {
        storage_ptr stg;
        if (ver == cfb_version::v4) ASSERT_TRUE(SUCCEEDED(win32_create_v4(path.wstring(), stg.put())));
        else ASSERT_TRUE(SUCCEEDED(win32_create_v3(path.wstring(), stg.put())));
        auto wname = std::wstring(stream_name.begin(), stream_name.end());
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(stg->CreateStream(wname.c_str(),
            STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, strm.put())));
        if (sz > 0) ASSERT_TRUE(SUCCEEDED(win32_stream_write(strm.get(), data.data(), static_cast<ULONG>(sz))));
    }
    auto cf = compound_file::open(path, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto s = cf->root_storage().open_stream(stream_name);
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->size(), static_cast<uint64_t>(sz));
    if (sz > 0) {
        std::vector<uint8_t> buf(sz);
        auto rd = s->read(0, std::span<uint8_t>(buf));
        ASSERT_TRUE(rd.has_value());
        EXPECT_EQ(buf, data);
    }
}

// ── Size sweep: Stout writes, Win32 reads ───────────────────────────────

TEST_P(StressStreamIOConformance, Size0) {
    auto p = temp_file("sio_0"); guard_.add(p);
    stout_write_win32_read(p, GetParam().ver, "S", 0, 0);
}
TEST_P(StressStreamIOConformance, Size1) {
    auto p = temp_file("sio_1"); guard_.add(p);
    stout_write_win32_read(p, GetParam().ver, "S", 1, 1);
}
TEST_P(StressStreamIOConformance, Size63) {
    auto p = temp_file("sio_63"); guard_.add(p);
    stout_write_win32_read(p, GetParam().ver, "S", 63, 2);
}
TEST_P(StressStreamIOConformance, Size64) {
    auto p = temp_file("sio_64"); guard_.add(p);
    stout_write_win32_read(p, GetParam().ver, "S", 64, 3);
}
TEST_P(StressStreamIOConformance, Size65) {
    auto p = temp_file("sio_65"); guard_.add(p);
    stout_write_win32_read(p, GetParam().ver, "S", 65, 4);
}
TEST_P(StressStreamIOConformance, Size255) {
    auto p = temp_file("sio_255"); guard_.add(p);
    stout_write_win32_read(p, GetParam().ver, "S", 255, 5);
}
TEST_P(StressStreamIOConformance, Size256) {
    auto p = temp_file("sio_256"); guard_.add(p);
    stout_write_win32_read(p, GetParam().ver, "S", 256, 6);
}
TEST_P(StressStreamIOConformance, Size511) {
    auto p = temp_file("sio_511"); guard_.add(p);
    stout_write_win32_read(p, GetParam().ver, "S", 511, 7);
}
TEST_P(StressStreamIOConformance, Size512) {
    auto p = temp_file("sio_512"); guard_.add(p);
    stout_write_win32_read(p, GetParam().ver, "S", 512, 8);
}
TEST_P(StressStreamIOConformance, Size513) {
    auto p = temp_file("sio_513"); guard_.add(p);
    stout_write_win32_read(p, GetParam().ver, "S", 513, 9);
}
TEST_P(StressStreamIOConformance, Size1023) {
    auto p = temp_file("sio_1023"); guard_.add(p);
    stout_write_win32_read(p, GetParam().ver, "S", 1023, 10);
}
TEST_P(StressStreamIOConformance, Size1024) {
    auto p = temp_file("sio_1024"); guard_.add(p);
    stout_write_win32_read(p, GetParam().ver, "S", 1024, 11);
}
TEST_P(StressStreamIOConformance, Size2048) {
    auto p = temp_file("sio_2048"); guard_.add(p);
    stout_write_win32_read(p, GetParam().ver, "S", 2048, 12);
}
TEST_P(StressStreamIOConformance, Size4095) {
    auto p = temp_file("sio_4095"); guard_.add(p);
    stout_write_win32_read(p, GetParam().ver, "S", 4095, 13);
}
TEST_P(StressStreamIOConformance, Size4096) {
    auto p = temp_file("sio_4096"); guard_.add(p);
    stout_write_win32_read(p, GetParam().ver, "S", 4096, 14);
}
TEST_P(StressStreamIOConformance, Size4097) {
    auto p = temp_file("sio_4097"); guard_.add(p);
    stout_write_win32_read(p, GetParam().ver, "S", 4097, 15);
}
TEST_P(StressStreamIOConformance, Size8191) {
    auto p = temp_file("sio_8191"); guard_.add(p);
    stout_write_win32_read(p, GetParam().ver, "S", 8191, 16);
}
TEST_P(StressStreamIOConformance, Size8192) {
    auto p = temp_file("sio_8192"); guard_.add(p);
    stout_write_win32_read(p, GetParam().ver, "S", 8192, 17);
}
TEST_P(StressStreamIOConformance, Size16384) {
    auto p = temp_file("sio_16k"); guard_.add(p);
    stout_write_win32_read(p, GetParam().ver, "S", 16384, 18);
}
TEST_P(StressStreamIOConformance, SizeLarge) {
    auto p = temp_file("sio_lg"); guard_.add(p);
    size_t sz = (GetParam().ver == cfb_version::v4) ? 65536 : 16384;
    stout_write_win32_read(p, GetParam().ver, "S", sz, 19);
}

// ── Size sweep: Win32 writes, Stout reads ───────────────────────────────

TEST_P(StressStreamIOConformance, W32Size0) {
    auto p = temp_file("siow_0"); guard_.add(p);
    win32_write_stout_read(p, GetParam().ver, "S", 0, 0);
}
TEST_P(StressStreamIOConformance, W32Size1) {
    auto p = temp_file("siow_1"); guard_.add(p);
    win32_write_stout_read(p, GetParam().ver, "S", 1, 1);
}
TEST_P(StressStreamIOConformance, W32Size64) {
    auto p = temp_file("siow_64"); guard_.add(p);
    win32_write_stout_read(p, GetParam().ver, "S", 64, 3);
}
TEST_P(StressStreamIOConformance, W32Size512) {
    auto p = temp_file("siow_512"); guard_.add(p);
    win32_write_stout_read(p, GetParam().ver, "S", 512, 8);
}
TEST_P(StressStreamIOConformance, W32Size4095) {
    auto p = temp_file("siow_4095"); guard_.add(p);
    win32_write_stout_read(p, GetParam().ver, "S", 4095, 13);
}
TEST_P(StressStreamIOConformance, W32Size4096) {
    auto p = temp_file("siow_4096"); guard_.add(p);
    win32_write_stout_read(p, GetParam().ver, "S", 4096, 14);
}
TEST_P(StressStreamIOConformance, W32Size4097) {
    auto p = temp_file("siow_4097"); guard_.add(p);
    win32_write_stout_read(p, GetParam().ver, "S", 4097, 15);
}
TEST_P(StressStreamIOConformance, W32Size8192) {
    auto p = temp_file("siow_8192"); guard_.add(p);
    win32_write_stout_read(p, GetParam().ver, "S", 8192, 17);
}
TEST_P(StressStreamIOConformance, W32Size65536) {
    auto p = temp_file("siow_64k"); guard_.add(p);
    win32_write_stout_read(p, GetParam().ver, "S", 65536, 19);
}

// ── Partial read ────────────────────────────────────────────────────────

TEST_P(StressStreamIOConformance, PartialReadFirstHalf) {
    auto p = temp_file("sio_ph1"); guard_.add(p);
    auto data = make_test_data(1000, 0xAA);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("D");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto s = cf->root_storage().open_stream("D");
    ASSERT_TRUE(s.has_value());
    std::vector<uint8_t> buf(500);
    auto rd = s->read(0, std::span<uint8_t>(buf));
    ASSERT_TRUE(rd.has_value());
    EXPECT_EQ(*rd, 500u);
    EXPECT_EQ(buf, std::vector<uint8_t>(data.begin(), data.begin() + 500));
}

TEST_P(StressStreamIOConformance, PartialReadSecondHalf) {
    auto p = temp_file("sio_ph2"); guard_.add(p);
    auto data = make_test_data(1000, 0xBB);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("D");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto s = cf->root_storage().open_stream("D");
    ASSERT_TRUE(s.has_value());
    std::vector<uint8_t> buf(500);
    auto rd = s->read(500, std::span<uint8_t>(buf));
    ASSERT_TRUE(rd.has_value());
    EXPECT_EQ(*rd, 500u);
    EXPECT_EQ(buf, std::vector<uint8_t>(data.begin() + 500, data.end()));
}

TEST_P(StressStreamIOConformance, PartialReadMiddle) {
    auto p = temp_file("sio_pmid"); guard_.add(p);
    auto data = make_test_data(2000, 0xCC);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("D");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto s = cf->root_storage().open_stream("D");
    ASSERT_TRUE(s.has_value());
    std::vector<uint8_t> buf(200);
    auto rd = s->read(800, std::span<uint8_t>(buf));
    ASSERT_TRUE(rd.has_value());
    EXPECT_EQ(*rd, 200u);
    EXPECT_EQ(buf, std::vector<uint8_t>(data.begin() + 800, data.begin() + 1000));
}

// ── Read beyond EOF ─────────────────────────────────────────────────────

TEST_P(StressStreamIOConformance, ReadBeyondEOF) {
    auto p = temp_file("sio_eof"); guard_.add(p);
    auto data = make_test_data(100, 0xDD);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("D");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto s = cf->root_storage().open_stream("D");
    ASSERT_TRUE(s.has_value());
    std::vector<uint8_t> buf(100);
    auto rd = s->read(200, std::span<uint8_t>(buf));
    ASSERT_TRUE(rd.has_value());
    EXPECT_EQ(*rd, 0u);
}

// ── Write at offset with gap ────────────────────────────────────────────

TEST_P(StressStreamIOConformance, WriteAtOffsetWithGap) {
    auto p = temp_file("sio_gap"); guard_.add(p);
    auto data = make_test_data(100, 0xEE);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("D");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->resize(500).has_value());
        ASSERT_TRUE(s->write(400, std::span<const uint8_t>(data)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"D", nullptr,
        STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 500u);
    // Seek to 400 and read
    LARGE_INTEGER li; li.QuadPart = 400;
    ASSERT_TRUE(SUCCEEDED(strm->Seek(li, STREAM_SEEK_SET, nullptr)));
    std::vector<uint8_t> buf(100);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 100, &rc)));
    EXPECT_EQ(buf, data);
}

// ── Overwrite middle ────────────────────────────────────────────────────

TEST_P(StressStreamIOConformance, OverwriteMiddle) {
    auto p = temp_file("sio_ovr"); guard_.add(p);
    auto data = make_test_data(500, 0x11);
    auto patch = make_test_data(100, 0xFF);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("D");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        ASSERT_TRUE(s->write(200, std::span<const uint8_t>(patch)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto s = cf->root_storage().open_stream("D");
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->size(), 500u);
    std::vector<uint8_t> buf(500);
    ASSERT_TRUE(s->read(0, std::span<uint8_t>(buf)).has_value());
    // First 200 bytes original
    EXPECT_EQ(std::vector<uint8_t>(buf.begin(), buf.begin() + 200),
              std::vector<uint8_t>(data.begin(), data.begin() + 200));
    // Middle 100 bytes patched
    EXPECT_EQ(std::vector<uint8_t>(buf.begin() + 200, buf.begin() + 300), patch);
    // Last 200 bytes original
    EXPECT_EQ(std::vector<uint8_t>(buf.begin() + 300, buf.end()),
              std::vector<uint8_t>(data.begin() + 300, data.end()));
}

// ── Multiple sequential writes ──────────────────────────────────────────

TEST_P(StressStreamIOConformance, MultipleSequentialWrites) {
    auto p = temp_file("sio_seq"); guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("D");
        ASSERT_TRUE(s.has_value());
        for (int i = 0; i < 10; ++i) {
            auto chunk = make_test_data(100, static_cast<uint8_t>(i));
            ASSERT_TRUE(s->write(i * 100, std::span<const uint8_t>(chunk)).has_value());
        }
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"D", nullptr,
        STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 1000u);
    for (int i = 0; i < 10; ++i) {
        LARGE_INTEGER li; li.QuadPart = i * 100;
        ASSERT_TRUE(SUCCEEDED(strm->Seek(li, STREAM_SEEK_SET, nullptr)));
        std::vector<uint8_t> buf(100);
        ULONG rc = 0;
        ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 100, &rc)));
        EXPECT_EQ(buf, make_test_data(100, static_cast<uint8_t>(i))) << "Chunk " << i;
    }
}

// ── Data patterns ───────────────────────────────────────────────────────

TEST_P(StressStreamIOConformance, AllZerosPattern) {
    auto p = temp_file("sio_zeros"); guard_.add(p);
    std::vector<uint8_t> data(500, 0x00);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("D");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"D", nullptr,
        STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    std::vector<uint8_t> buf(500);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 500, &rc)));
    EXPECT_EQ(buf, data);
}

TEST_P(StressStreamIOConformance, AllOnesPattern) {
    auto p = temp_file("sio_ones"); guard_.add(p);
    std::vector<uint8_t> data(500, 0xFF);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("D");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"D", nullptr,
        STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    std::vector<uint8_t> buf(500);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 500, &rc)));
    EXPECT_EQ(buf, data);
}

TEST_P(StressStreamIOConformance, ByteValueSweep) {
    auto p = temp_file("sio_sweep"); guard_.add(p);
    std::vector<uint8_t> data(256);
    std::iota(data.begin(), data.end(), uint8_t(0));
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("D");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"D", nullptr,
        STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    std::vector<uint8_t> buf(256);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 256, &rc)));
    EXPECT_EQ(buf, data);
}

// ── Large stream 1MB ────────────────────────────────────────────────────

TEST_P(StressStreamIOConformance, LargeStream) {
    auto p = temp_file("sio_large"); guard_.add(p);
    // V3 with 512-byte sectors has FAT chain limits; use smaller size
    size_t sz = (GetParam().ver == cfb_version::v4) ? 1024 * 1024 : 32768;
    stout_write_win32_read(p, GetParam().ver, "Big", sz, 42);
}

// ── Write 0 bytes (no-op) ───────────────────────────────────────────────

TEST_P(StressStreamIOConformance, WriteZeroBytes) {
    auto p = temp_file("sio_wz"); guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("D");
        ASSERT_TRUE(s.has_value());
        std::vector<uint8_t> empty;
        auto wr = s->write(0, std::span<const uint8_t>(empty));
        ASSERT_TRUE(wr.has_value());
        EXPECT_EQ(s->size(), 0u);
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"D", nullptr,
        STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 0u);
}

// ── Append at end ───────────────────────────────────────────────────────

TEST_P(StressStreamIOConformance, AppendAtEnd) {
    auto p = temp_file("sio_append"); guard_.add(p);
    auto data1 = make_test_data(200, 0x11);
    auto data2 = make_test_data(300, 0x22);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("D");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data1)).has_value());
        ASSERT_TRUE(s->write(200, std::span<const uint8_t>(data2)).has_value());
        EXPECT_EQ(s->size(), 500u);
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto s = cf->root_storage().open_stream("D");
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->size(), 500u);
    std::vector<uint8_t> buf(500);
    ASSERT_TRUE(s->read(0, std::span<uint8_t>(buf)).has_value());
    EXPECT_EQ(std::vector<uint8_t>(buf.begin(), buf.begin() + 200), data1);
    EXPECT_EQ(std::vector<uint8_t>(buf.begin() + 200, buf.end()), data2);
}

#endif // _WIN32
