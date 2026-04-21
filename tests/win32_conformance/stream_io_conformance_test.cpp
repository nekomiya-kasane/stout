#ifdef _WIN32

#    include "conformance_utils.h"

#    include <algorithm>
#    include <gtest/gtest.h>
#    include <numeric>
#    include <stout/compound_file.h>

using namespace conformance;
using namespace stout;

class StreamIOConformance : public ::testing::Test {
  protected:
    com_init com_;
    temp_file_guard guard_;
};

// ── WriteSmallStream: cross-read 100 bytes ──────────────────────────────
TEST_F(StreamIOConformance, WriteSmallStream) {
    auto [stout_path, win32_path] = temp_file_pair("sm_stream");
    guard_.add(stout_path);
    guard_.add(win32_path);

    auto data = make_test_data(100);

    // Stout writes
    {
        auto cf = compound_file::create(stout_path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        auto s = root.create_stream("TestData");
        ASSERT_TRUE(s.has_value()) << error_message(s.error());
        auto wr = s->write(0, std::span<const uint8_t>(data));
        ASSERT_TRUE(wr.has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }

    // Win32 reads Stout's file
    {
        storage_ptr stg;
        ASSERT_TRUE(SUCCEEDED(win32_open_read(stout_path.wstring(), stg.put())));
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"TestData", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
        std::vector<uint8_t> buf(100);
        ULONG read_count = 0;
        ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 100, &read_count)));
        EXPECT_EQ(read_count, 100u);
        EXPECT_EQ(buf, data);
    }

    // Win32 writes identical data
    {
        storage_ptr stg;
        ASSERT_TRUE(SUCCEEDED(win32_create_v4(win32_path.wstring(), stg.put())));
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(
            stg->CreateStream(L"TestData", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, strm.put())));
        ASSERT_TRUE(SUCCEEDED(win32_stream_write(strm.get(), data.data(), 100)));
    }

    // Stout reads Win32's file
    {
        auto cf = compound_file::open(win32_path, open_mode::read);
        ASSERT_TRUE(cf.has_value()) << error_message(cf.error());
        auto root = cf->root_storage();
        auto s = root.open_stream("TestData");
        ASSERT_TRUE(s.has_value()) << error_message(s.error());
        EXPECT_EQ(s->size(), 100u);
        std::vector<uint8_t> buf(100);
        auto rd = s->read(0, std::span<uint8_t>(buf));
        ASSERT_TRUE(rd.has_value());
        EXPECT_EQ(*rd, 100u);
        EXPECT_EQ(buf, data);
    }
}

// ── WriteLargeStream: 8KB (above mini-stream cutoff) ────────────────────
TEST_F(StreamIOConformance, WriteLargeStream) {
    auto [stout_path, win32_path] = temp_file_pair("lg_stream");
    guard_.add(stout_path);
    guard_.add(win32_path);

    auto data = make_test_data(8192, 42);

    // Stout writes
    {
        auto cf = compound_file::create(stout_path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        auto s = root.create_stream("BigData");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }

    // Win32 reads Stout's file
    {
        storage_ptr stg;
        ASSERT_TRUE(SUCCEEDED(win32_open_read(stout_path.wstring(), stg.put())));
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"BigData", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
        EXPECT_EQ(win32_stream_size(strm.get()), 8192u);
        std::vector<uint8_t> buf(8192);
        ULONG read_count = 0;
        ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 8192, &read_count)));
        EXPECT_EQ(read_count, 8192u);
        EXPECT_EQ(buf, data);
    }

    // Win32 writes, Stout reads
    {
        storage_ptr stg;
        ASSERT_TRUE(SUCCEEDED(win32_create_v4(win32_path.wstring(), stg.put())));
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(
            stg->CreateStream(L"BigData", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, strm.put())));
        ASSERT_TRUE(SUCCEEDED(win32_stream_write(strm.get(), data.data(), 8192)));
    }
    {
        auto cf = compound_file::open(win32_path, open_mode::read);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().open_stream("BigData");
        ASSERT_TRUE(s.has_value());
        EXPECT_EQ(s->size(), 8192u);
        std::vector<uint8_t> buf(8192);
        auto rd = s->read(0, std::span<uint8_t>(buf));
        ASSERT_TRUE(rd.has_value());
        EXPECT_EQ(buf, data);
    }
}

// ── EmptyStream: create stream, write nothing ───────────────────────────
TEST_F(StreamIOConformance, EmptyStream) {
    auto path = temp_file("empty_strm");
    guard_.add(path);

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("Empty");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }

    // Win32 reads
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"Empty", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 0u);
}

// ── BinaryData: write bytes 0x00–0xFF pattern ───────────────────────────
TEST_F(StreamIOConformance, BinaryData) {
    auto path = temp_file("binary_data");
    guard_.add(path);

    std::vector<uint8_t> data(256);
    std::iota(data.begin(), data.end(), uint8_t(0));

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("Binary");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }

    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"Binary", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    std::vector<uint8_t> buf(256);
    ULONG read_count = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 256, &read_count)));
    EXPECT_EQ(read_count, 256u);
    EXPECT_EQ(buf, data);
}

// ── LargeStreamMultiSector: write 1 MB ──────────────────────────────────
TEST_F(StreamIOConformance, LargeStreamMultiSector) {
    auto path = temp_file("large_1mb");
    guard_.add(path);

    auto data = make_test_data(1024 * 1024, 7);

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("OneMB");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }

    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"OneMB", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 1024u * 1024u);

    std::vector<uint8_t> buf(1024 * 1024);
    ULONG read_count = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), static_cast<ULONG>(buf.size()), &read_count)));
    EXPECT_EQ(read_count, static_cast<ULONG>(data.size()));
    EXPECT_EQ(buf, data);
}

// ── SetSizeGrow: resize then write at offset ────────────────────────────
TEST_F(StreamIOConformance, SetSizeGrow) {
    auto path = temp_file("grow_stream");
    guard_.add(path);

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("Grow");
        ASSERT_TRUE(s.has_value());
        // Resize to 8192
        ASSERT_TRUE(s->resize(8192).has_value());
        // Write at offset 4096
        auto data = make_test_data(100, 0xAB);
        ASSERT_TRUE(s->write(4096, std::span<const uint8_t>(data)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }

    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"Grow", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 8192u);

    // Seek to 4096 and read 100 bytes
    LARGE_INTEGER li;
    li.QuadPart = 4096;
    ASSERT_TRUE(SUCCEEDED(strm->Seek(li, STREAM_SEEK_SET, nullptr)));
    std::vector<uint8_t> buf(100);
    ULONG read_count = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 100, &read_count)));
    EXPECT_EQ(read_count, 100u);
    auto expected = make_test_data(100, 0xAB);
    EXPECT_EQ(buf, expected);
}

// ── MultipleStreams: 5 streams with different data ───────────────────────
TEST_F(StreamIOConformance, MultipleStreams) {
    auto path = temp_file("multi_strm");
    guard_.add(path);

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        for (int i = 0; i < 5; ++i) {
            auto name = "Stream" + std::to_string(i);
            auto s = root.create_stream(name);
            ASSERT_TRUE(s.has_value()) << "Failed to create " << name;
            auto data = make_test_data(200 + i * 100, static_cast<uint8_t>(i));
            ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        }
        ASSERT_TRUE(cf->flush().has_value());
    }

    // Win32 reads all streams
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    for (int i = 0; i < 5; ++i) {
        auto name = L"Stream" + std::to_wstring(i);
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(stg->OpenStream(name.c_str(), nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())))
            << "Win32 failed to open " << i;
        auto expected_size = 200 + i * 100;
        EXPECT_EQ(win32_stream_size(strm.get()), static_cast<uint64_t>(expected_size));

        std::vector<uint8_t> buf(expected_size);
        ULONG read_count = 0;
        ASSERT_TRUE(
            SUCCEEDED(win32_stream_read(strm.get(), buf.data(), static_cast<ULONG>(expected_size), &read_count)));
        auto expected = make_test_data(expected_size, static_cast<uint8_t>(i));
        EXPECT_EQ(buf, expected) << "Data mismatch for stream " << i;
    }
}

#endif // _WIN32
