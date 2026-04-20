#ifdef _WIN32

#include "conformance_utils.h"

#include <gtest/gtest.h>
#include <stout/compound_file.h>

using namespace conformance;
using namespace stout;

class ResizeConformance : public ::testing::Test {
  protected:
    com_init com_;
    temp_file_guard guard_;
};

// ── Mini-to-regular migration preserves data (Stout write, Win32 read) ──

TEST_F(ResizeConformance, MiniToRegularPreservesData) {
    auto path = temp_file("resize_m2r");
    guard_.add(path);

    std::vector<uint8_t> data = make_test_data(100);
    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        auto s = root.create_stream("TestStream");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());

        // Resize past mini cutoff (4096)
        ASSERT_TRUE(s->resize(5000).has_value());

        // Verify original data still readable
        std::vector<uint8_t> readback(100);
        ASSERT_TRUE(s->read(0, std::span<uint8_t>(readback)).has_value());
        EXPECT_EQ(readback, data);

        ASSERT_TRUE(cf->flush().has_value());
    }

    // Win32 reads
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"TestStream", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 5000u);

    std::vector<uint8_t> w32_buf(100);
    ULONG actual = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), w32_buf.data(), static_cast<ULONG>(w32_buf.size()), &actual)));
    EXPECT_EQ(actual, 100u);
    EXPECT_EQ(w32_buf, data);
}

// ── Regular-to-mini migration preserves data ──

TEST_F(ResizeConformance, RegularToMiniPreservesData) {
    auto path = temp_file("resize_r2m");
    guard_.add(path);

    std::vector<uint8_t> data = make_test_data(5000);
    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        auto s = root.create_stream("BigStream");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());

        // Resize down to mini
        ASSERT_TRUE(s->resize(200).has_value());

        std::vector<uint8_t> readback(200);
        ASSERT_TRUE(s->read(0, std::span<uint8_t>(readback)).has_value());
        std::vector<uint8_t> expected(data.begin(), data.begin() + 200);
        EXPECT_EQ(readback, expected);

        ASSERT_TRUE(cf->flush().has_value());
    }

    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"BigStream", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 200u);

    std::vector<uint8_t> w32_buf(200);
    ULONG actual = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), w32_buf.data(), static_cast<ULONG>(w32_buf.size()), &actual)));
    std::vector<uint8_t> expected(data.begin(), data.begin() + 200);
    EXPECT_EQ(w32_buf, expected);
}

// ── Mini-to-regular at exact cutoff boundary ──

TEST_F(ResizeConformance, MiniToRegularAtExactCutoff) {
    auto path = temp_file("resize_cutoff");
    guard_.add(path);

    std::vector<uint8_t> data = make_test_data(4000);
    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("BoundaryStream");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());

        // Resize to exactly 4096 (the cutoff)
        ASSERT_TRUE(s->resize(4096).has_value());

        std::vector<uint8_t> readback(4000);
        ASSERT_TRUE(s->read(0, std::span<uint8_t>(readback)).has_value());
        EXPECT_EQ(readback, data);

        ASSERT_TRUE(cf->flush().has_value());
    }

    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(
        SUCCEEDED(stg->OpenStream(L"BoundaryStream", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 4096u);

    std::vector<uint8_t> w32_buf(4000);
    ULONG actual = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), w32_buf.data(), static_cast<ULONG>(w32_buf.size()), &actual)));
    EXPECT_EQ(w32_buf, data);
}

// ── Win32 creates large stream, Stout resizes to mini ──

TEST_F(ResizeConformance, Win32CreateLarge_StoutResizeToMini) {
    auto path = temp_file("resize_w32_r2m");
    guard_.add(path);

    std::vector<uint8_t> data = make_test_data(8000);

    // Win32 creates
    {
        storage_ptr stg;
        ASSERT_TRUE(SUCCEEDED(win32_create_v4(path.wstring(), stg.put())));
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(
            stg->CreateStream(L"LargeStream", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, strm.put())));
        ASSERT_TRUE(SUCCEEDED(win32_stream_write(strm.get(), data.data(), static_cast<ULONG>(data.size()))));
    }

    // Stout resizes to mini
    {
        auto cf = compound_file::open(path, open_mode::read_write);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().open_stream("LargeStream");
        ASSERT_TRUE(s.has_value());
        EXPECT_EQ(s->size(), 8000u);

        ASSERT_TRUE(s->resize(500).has_value());

        std::vector<uint8_t> readback(500);
        ASSERT_TRUE(s->read(0, std::span<uint8_t>(readback)).has_value());
        std::vector<uint8_t> expected(data.begin(), data.begin() + 500);
        EXPECT_EQ(readback, expected);

        ASSERT_TRUE(cf->flush().has_value());
    }

    // Win32 verifies
    {
        storage_ptr stg;
        ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
        stream_ptr strm;
        ASSERT_TRUE(
            SUCCEEDED(stg->OpenStream(L"LargeStream", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
        EXPECT_EQ(win32_stream_size(strm.get()), 500u);

        std::vector<uint8_t> w32_buf(500);
        ULONG actual = 0;
        ASSERT_TRUE(
            SUCCEEDED(win32_stream_read(strm.get(), w32_buf.data(), static_cast<ULONG>(w32_buf.size()), &actual)));
        std::vector<uint8_t> expected(data.begin(), data.begin() + 500);
        EXPECT_EQ(w32_buf, expected);
    }
}

// ── Stout mini resize to regular, Win32 reads ──

TEST_F(ResizeConformance, StoutMiniResizeToRegular_Win32Reads) {
    auto path = temp_file("resize_grow");
    guard_.add(path);

    std::vector<uint8_t> mini_data = make_test_data(64);
    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("GrowStream");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(mini_data)).has_value());

        // Grow past cutoff
        ASSERT_TRUE(s->resize(10000).has_value());

        // Write additional data at offset 5000
        std::vector<uint8_t> extra = make_test_data(100);
        ASSERT_TRUE(s->write(5000, std::span<const uint8_t>(extra)).has_value());

        ASSERT_TRUE(cf->flush().has_value());
    }

    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"GrowStream", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 10000u);

    // First 64 bytes should be the original mini data
    std::vector<uint8_t> w32_buf(64);
    ULONG actual = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), w32_buf.data(), static_cast<ULONG>(w32_buf.size()), &actual)));
    EXPECT_EQ(w32_buf, mini_data);
}

#endif // _WIN32
