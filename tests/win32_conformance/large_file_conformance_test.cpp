#ifdef _WIN32

#include "conformance_utils.h"

#include <gtest/gtest.h>
#include <set>
#include <stout/compound_file.h>

using namespace conformance;
using namespace stout;

class LargeFileConformance : public ::testing::Test {
  protected:
    com_init com_;
    temp_file_guard guard_;
};

// ── ManyStreams100: create 100 streams with data ────────────────────────
TEST_F(LargeFileConformance, ManyStreams100) {
    auto path = temp_file("lf_100");
    guard_.add(path);

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        for (int i = 0; i < 100; ++i) {
            auto s = root.create_stream("S" + std::to_string(i));
            ASSERT_TRUE(s.has_value()) << "Failed to create stream S" << i;
            auto data = make_test_data(100, static_cast<uint8_t>(i));
            ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        }
        ASSERT_TRUE(cf->flush().has_value());
    }

    // Win32 enumerates all 100
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    auto entries = win32_enumerate(stg.get());
    EXPECT_EQ(entries.size(), 100u);

    // Spot-check a few streams
    for (int i : {0, 49, 99}) {
        auto name = L"S" + std::to_wstring(i);
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(stg->OpenStream(name.c_str(), nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
        EXPECT_EQ(win32_stream_size(strm.get()), 100u);
        std::vector<uint8_t> buf(100);
        ULONG rc = 0;
        ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 100, &rc)));
        auto expected = make_test_data(100, static_cast<uint8_t>(i));
        EXPECT_EQ(buf, expected) << "Mismatch at stream S" << i;
    }

    for (auto &e : entries) {
        free_statstg_name(e);
    }
}

// ── LargeStream1MB: single 1 MB stream ─────────────────────────────────
TEST_F(LargeFileConformance, LargeStream1MB) {
    auto path = temp_file("lf_1mb");
    guard_.add(path);

    auto data = make_test_data(1024 * 1024, 0x42);
    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("BigData");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }

    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"BigData", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 1024u * 1024u);

    // Read in chunks and verify
    std::vector<uint8_t> buf(1024 * 1024);
    ULONG total_read = 0;
    ULONG chunk_read = 0;
    while (total_read < 1024 * 1024) {
        ULONG to_read = std::min(ULONG(65536), ULONG(1024 * 1024 - total_read));
        ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data() + total_read, to_read, &chunk_read)));
        if (chunk_read == 0) {
            break;
        }
        total_read += chunk_read;
    }
    EXPECT_EQ(total_read, 1024u * 1024u);
    EXPECT_EQ(buf, data);
}

// ── ManyStorages50: 50 storages, each with a stream ─────────────────────
TEST_F(LargeFileConformance, ManyStorages50) {
    auto path = temp_file("lf_50stg");
    guard_.add(path);

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        for (int i = 0; i < 50; ++i) {
            auto sub = root.create_storage("Dir" + std::to_string(i));
            ASSERT_TRUE(sub.has_value()) << "Failed at Dir" << i;
            auto s = sub->create_stream("File");
            ASSERT_TRUE(s.has_value());
            auto data = make_test_data(50, static_cast<uint8_t>(i));
            ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        }
        ASSERT_TRUE(cf->flush().has_value());
    }

    // Win32 enumerates 50 storages
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    auto entries = win32_enumerate(stg.get());
    EXPECT_EQ(entries.size(), 50u);

    // Spot-check a few
    for (int i : {0, 25, 49}) {
        auto name = L"Dir" + std::to_wstring(i);
        storage_ptr sub;
        ASSERT_TRUE(SUCCEEDED(
            stg->OpenStorage(name.c_str(), nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0, sub.put())));
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(sub->OpenStream(L"File", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
        EXPECT_EQ(win32_stream_size(strm.get()), 50u);
    }

    for (auto &e : entries) {
        free_statstg_name(e);
    }
}

// ── DirectorySectorGrowth: enough entries to span multiple dir sectors ──
TEST_F(LargeFileConformance, DirectorySectorGrowth) {
    auto path = temp_file("lf_dirgrow");
    guard_.add(path);

    // v4 sector = 4096 bytes, each dir entry = 128 bytes → 32 entries/sector
    // Root entry takes 1 slot, so 64 children should require 2+ dir sectors
    const int count = 64;
    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        for (int i = 0; i < count; ++i) {
            auto s = root.create_stream("E" + std::to_string(i));
            ASSERT_TRUE(s.has_value()) << "Failed at E" << i;
        }
        ASSERT_TRUE(cf->flush().has_value());
    }

    // Win32 enumerates all
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    auto entries = win32_enumerate(stg.get());
    EXPECT_EQ(entries.size(), static_cast<size_t>(count));
    for (auto &e : entries) {
        free_statstg_name(e);
    }

    // Stout also enumerates all
    auto cf = compound_file::open(path, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    EXPECT_EQ(cf->root_storage().children().size(), static_cast<size_t>(count));
}

// ── Win32 creates large, Stout reads ────────────────────────────────────
TEST_F(LargeFileConformance, Win32LargeStoutRead) {
    auto path = temp_file("lf_w32big");
    guard_.add(path);

    const size_t sz = 50000;
    auto data = make_test_data(sz, 0xEE);
    {
        storage_ptr stg;
        ASSERT_TRUE(SUCCEEDED(win32_create_v4(path.wstring(), stg.put())));
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(
            stg->CreateStream(L"HugeStream", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, strm.put())));
        ASSERT_TRUE(SUCCEEDED(win32_stream_write(strm.get(), data.data(), static_cast<ULONG>(sz))));
    }

    auto cf = compound_file::open(path, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto s = cf->root_storage().open_stream("HugeStream");
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->size(), sz);

    std::vector<uint8_t> buf(sz);
    auto rd = s->read(0, std::span<uint8_t>(buf));
    ASSERT_TRUE(rd.has_value()) << "read failed";
    EXPECT_EQ(*rd, sz);
    EXPECT_EQ(buf, data);
}

#endif // _WIN32
