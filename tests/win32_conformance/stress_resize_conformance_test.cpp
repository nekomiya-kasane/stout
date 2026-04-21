#ifdef _WIN32

#include "conformance_utils.h"

#include <gtest/gtest.h>
#include <stout/compound_file.h>

using namespace conformance;
using namespace stout;

struct VersionParam4 {
    cfb_version ver;
    uint16_t major;
};

class StressResizeConformance : public ::testing::TestWithParam<VersionParam4> {
  protected:
    com_init com_;
    temp_file_guard guard_;
};

static const VersionParam4 versions4[] = {
    {cfb_version::v3, 3},
    {cfb_version::v4, 4},
};

INSTANTIATE_TEST_SUITE_P(V, StressResizeConformance, ::testing::ValuesIn(versions4),
                         [](const auto &info) { return info.param.major == 3 ? "V3" : "V4"; });

// Helper: create stream with data, resize, verify with Win32
static void resize_and_verify(const std::filesystem::path &path, cfb_version ver, size_t initial_sz, size_t new_sz,
                              uint8_t seed) {
    auto data = make_test_data(initial_sz, seed);
    {
        auto cf = compound_file::create(path, ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("R");
        ASSERT_TRUE(s.has_value());
        if (initial_sz > 0) {
            ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        }
        ASSERT_TRUE(s->resize(new_sz).has_value());
        EXPECT_EQ(s->size(), new_sz);
        // Verify preserved data
        auto preserved = std::min(initial_sz, new_sz);
        if (preserved > 0) {
            std::vector<uint8_t> buf(preserved);
            auto rd = s->read(0, std::span<uint8_t>(buf));
            ASSERT_TRUE(rd.has_value());
            EXPECT_EQ(buf, std::vector<uint8_t>(data.begin(), data.begin() + preserved));
        }
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"R", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), static_cast<uint64_t>(new_sz));
    auto preserved = std::min(initial_sz, new_sz);
    if (preserved > 0) {
        std::vector<uint8_t> buf(preserved);
        ULONG rc = 0;
        ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), static_cast<ULONG>(preserved), &rc)));
        EXPECT_EQ(buf, std::vector<uint8_t>(data.begin(), data.begin() + preserved));
    }
}

// ── Resize from 0 ──────────────────────────────────────────────────────

TEST_P(StressResizeConformance, From0To1) {
    auto p = temp_file("sr_0_1");
    guard_.add(p);
    resize_and_verify(p, GetParam().ver, 0, 1, 1);
}
TEST_P(StressResizeConformance, From0To63) {
    auto p = temp_file("sr_0_63");
    guard_.add(p);
    resize_and_verify(p, GetParam().ver, 0, 63, 2);
}
TEST_P(StressResizeConformance, From0To64) {
    auto p = temp_file("sr_0_64");
    guard_.add(p);
    resize_and_verify(p, GetParam().ver, 0, 64, 3);
}
TEST_P(StressResizeConformance, From0To4095) {
    auto p = temp_file("sr_0_4095");
    guard_.add(p);
    resize_and_verify(p, GetParam().ver, 0, 4095, 4);
}
TEST_P(StressResizeConformance, From0To4096) {
    auto p = temp_file("sr_0_4096");
    guard_.add(p);
    resize_and_verify(p, GetParam().ver, 0, 4096, 5);
}
TEST_P(StressResizeConformance, From0To4097) {
    auto p = temp_file("sr_0_4097");
    guard_.add(p);
    resize_and_verify(p, GetParam().ver, 0, 4097, 6);
}
TEST_P(StressResizeConformance, From0To8192) {
    auto p = temp_file("sr_0_8192");
    guard_.add(p);
    resize_and_verify(p, GetParam().ver, 0, 8192, 7);
}

// ── Mini to mini (grow) ─────────────────────────────────────────────────

TEST_P(StressResizeConformance, Mini100ToMini200) {
    auto p = temp_file("sr_m100_200");
    guard_.add(p);
    resize_and_verify(p, GetParam().ver, 100, 200, 10);
}
TEST_P(StressResizeConformance, Mini100ToMini4095) {
    auto p = temp_file("sr_m100_4095");
    guard_.add(p);
    resize_and_verify(p, GetParam().ver, 100, 4095, 11);
}

// ── Mini to mini (shrink) ───────────────────────────────────────────────

TEST_P(StressResizeConformance, Mini4095ToMini100) {
    auto p = temp_file("sr_m4095_100");
    guard_.add(p);
    resize_and_verify(p, GetParam().ver, 4095, 100, 20);
}
TEST_P(StressResizeConformance, Mini4095ToMini1) {
    auto p = temp_file("sr_m4095_1");
    guard_.add(p);
    resize_and_verify(p, GetParam().ver, 4095, 1, 21);
}
TEST_P(StressResizeConformance, Mini100To0) {
    auto p = temp_file("sr_m100_0");
    guard_.add(p);
    resize_and_verify(p, GetParam().ver, 100, 0, 22);
}

// ── Mini to regular ─────────────────────────────────────────────────────

TEST_P(StressResizeConformance, Mini100ToReg4096) {
    auto p = temp_file("sr_m100_r4096");
    guard_.add(p);
    resize_and_verify(p, GetParam().ver, 100, 4096, 30);
}
TEST_P(StressResizeConformance, Mini100ToReg4097) {
    auto p = temp_file("sr_m100_r4097");
    guard_.add(p);
    resize_and_verify(p, GetParam().ver, 100, 4097, 31);
}
TEST_P(StressResizeConformance, Mini100ToReg8192) {
    auto p = temp_file("sr_m100_r8192");
    guard_.add(p);
    resize_and_verify(p, GetParam().ver, 100, 8192, 32);
}
TEST_P(StressResizeConformance, Mini100ToRegLarge) {
    auto p = temp_file("sr_m100_rlg");
    guard_.add(p);
    // V3 512-byte sectors have FAT chain limits for very large streams
    size_t target = (GetParam().ver == cfb_version::v4) ? 65536 : 16384;
    resize_and_verify(p, GetParam().ver, 100, target, 33);
}
TEST_P(StressResizeConformance, Mini2000ToReg5000) {
    auto p = temp_file("sr_m2k_r5k");
    guard_.add(p);
    resize_and_verify(p, GetParam().ver, 2000, 5000, 34);
}

// ── Regular to mini ─────────────────────────────────────────────────────

TEST_P(StressResizeConformance, Reg8192ToMini4095) {
    auto p = temp_file("sr_r8k_m4095");
    guard_.add(p);
    resize_and_verify(p, GetParam().ver, 8192, 4095, 40);
}
TEST_P(StressResizeConformance, Reg8192ToMini100) {
    auto p = temp_file("sr_r8k_m100");
    guard_.add(p);
    resize_and_verify(p, GetParam().ver, 8192, 100, 41);
}
TEST_P(StressResizeConformance, Reg8192ToMini1) {
    auto p = temp_file("sr_r8k_m1");
    guard_.add(p);
    resize_and_verify(p, GetParam().ver, 8192, 1, 42);
}
TEST_P(StressResizeConformance, Reg8192To0) {
    auto p = temp_file("sr_r8k_0");
    guard_.add(p);
    resize_and_verify(p, GetParam().ver, 8192, 0, 43);
}
TEST_P(StressResizeConformance, RegLargeToMini500) {
    auto p = temp_file("sr_rlg_m500");
    guard_.add(p);
    size_t initial = (GetParam().ver == cfb_version::v4) ? 65536 : 16384;
    resize_and_verify(p, GetParam().ver, initial, 500, 44);
}

// ── Regular to regular ──────────────────────────────────────────────────

TEST_P(StressResizeConformance, Reg4096ToReg8192) {
    auto p = temp_file("sr_r4k_r8k");
    guard_.add(p);
    resize_and_verify(p, GetParam().ver, 4096, 8192, 50);
}
TEST_P(StressResizeConformance, Reg4096ToRegLarge) {
    auto p = temp_file("sr_r4k_rlg");
    guard_.add(p);
    size_t target = (GetParam().ver == cfb_version::v4) ? 65536 : 16384;
    resize_and_verify(p, GetParam().ver, 4096, target, 51);
}
TEST_P(StressResizeConformance, RegLargeToReg4096) {
    auto p = temp_file("sr_rlg_r4k");
    guard_.add(p);
    size_t initial = (GetParam().ver == cfb_version::v4) ? 65536 : 16384;
    resize_and_verify(p, GetParam().ver, initial, 4096, 52);
}
TEST_P(StressResizeConformance, RegLargeToReg4097) {
    auto p = temp_file("sr_rlg_r4097");
    guard_.add(p);
    size_t initial = (GetParam().ver == cfb_version::v4) ? 65536 : 16384;
    resize_and_verify(p, GetParam().ver, initial, 4097, 53);
}

// ── Resize to same size (no-op) ─────────────────────────────────────────

TEST_P(StressResizeConformance, SameSizeMini) {
    auto p = temp_file("sr_same_m");
    guard_.add(p);
    resize_and_verify(p, GetParam().ver, 500, 500, 60);
}
TEST_P(StressResizeConformance, SameSizeRegular) {
    auto p = temp_file("sr_same_r");
    guard_.add(p);
    resize_and_verify(p, GetParam().ver, 8192, 8192, 61);
}

// ── Multiple resizes in sequence ────────────────────────────────────────

TEST_P(StressResizeConformance, MultipleResizesSequence) {
    auto p = temp_file("sr_multi");
    guard_.add(p);
    auto data = make_test_data(100, 0xAA);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("R");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        // mini -> regular
        ASSERT_TRUE(s->resize(5000).has_value());
        EXPECT_EQ(s->size(), 5000u);
        // regular -> mini
        ASSERT_TRUE(s->resize(200).has_value());
        EXPECT_EQ(s->size(), 200u);
        // mini -> regular again
        ASSERT_TRUE(s->resize(10000).has_value());
        EXPECT_EQ(s->size(), 10000u);
        // Verify original 100 bytes
        std::vector<uint8_t> buf(100);
        ASSERT_TRUE(s->read(0, std::span<uint8_t>(buf)).has_value());
        EXPECT_EQ(buf, data);
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"R", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 10000u);
    std::vector<uint8_t> buf(100);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 100, &rc)));
    EXPECT_EQ(buf, data);
}

// ── Resize with multiple streams (others unaffected) ────────────────────

TEST_P(StressResizeConformance, ResizeDoesNotAffectOthers) {
    auto p = temp_file("sr_others");
    guard_.add(p);
    auto data_a = make_test_data(200, 0x11);
    auto data_b = make_test_data(300, 0x22);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        auto sa = root.create_stream("A");
        ASSERT_TRUE(sa.has_value());
        ASSERT_TRUE(sa->write(0, std::span<const uint8_t>(data_a)).has_value());
        auto sb = root.create_stream("B");
        ASSERT_TRUE(sb.has_value());
        ASSERT_TRUE(sb->write(0, std::span<const uint8_t>(data_b)).has_value());
        // Resize A to regular
        ASSERT_TRUE(sa->resize(5000).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    // B should be unaffected
    stream_ptr sb;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"B", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, sb.put())));
    EXPECT_EQ(win32_stream_size(sb.get()), 300u);
    std::vector<uint8_t> buf(300);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(sb.get(), buf.data(), 300, &rc)));
    EXPECT_EQ(buf, data_b);
    // A should be 5000
    stream_ptr sa;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"A", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, sa.put())));
    EXPECT_EQ(win32_stream_size(sa.get()), 5000u);
}

// ── Win32 creates, Stout resizes, Win32 verifies ────────────────────────

TEST_P(StressResizeConformance, Win32CreateStoutResizeWin32Verify) {
    auto p = temp_file("sr_w32");
    guard_.add(p);
    auto data = make_test_data(6000, 0x55);
    {
        storage_ptr stg;
        if (GetParam().ver == cfb_version::v4) {
            ASSERT_TRUE(SUCCEEDED(win32_create_v4(p.wstring(), stg.put())));
        } else {
            ASSERT_TRUE(SUCCEEDED(win32_create_v3(p.wstring(), stg.put())));
        }
        stream_ptr strm;
        ASSERT_TRUE(
            SUCCEEDED(stg->CreateStream(L"R", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, strm.put())));
        ASSERT_TRUE(SUCCEEDED(win32_stream_write(strm.get(), data.data(), static_cast<ULONG>(data.size()))));
    }
    {
        auto cf = compound_file::open(p, open_mode::read_write);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().open_stream("R");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->resize(300).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    {
        storage_ptr stg;
        ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"R", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
        EXPECT_EQ(win32_stream_size(strm.get()), 300u);
        std::vector<uint8_t> buf(300);
        ULONG rc = 0;
        ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 300, &rc)));
        EXPECT_EQ(buf, std::vector<uint8_t>(data.begin(), data.begin() + 300));
    }
}

// ── Sector-aligned resize sizes ─────────────────────────────────────────

TEST_P(StressResizeConformance, ResizeToExactSectorSize) {
    auto p = temp_file("sr_sector");
    guard_.add(p);
    auto ss = GetParam().ver == cfb_version::v4 ? 4096u : 512u;
    resize_and_verify(p, GetParam().ver, 100, ss, 70);
}

TEST_P(StressResizeConformance, ResizeToTwoSectors) {
    auto p = temp_file("sr_2sec");
    guard_.add(p);
    auto ss = GetParam().ver == cfb_version::v4 ? 4096u : 512u;
    resize_and_verify(p, GetParam().ver, 100, ss * 2, 71);
}

#endif // _WIN32
