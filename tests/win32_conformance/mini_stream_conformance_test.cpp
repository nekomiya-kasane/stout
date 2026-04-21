#ifdef _WIN32

#    include "conformance_utils.h"

#    include <gtest/gtest.h>
#    include <stout/compound_file.h>

using namespace conformance;
using namespace stout;

class MiniStreamConformance : public ::testing::Test {
  protected:
    com_init com_;
    temp_file_guard guard_;
};

// ── SmallStreamInMini: write 100 bytes (below 4096 cutoff) ──────────────
TEST_F(MiniStreamConformance, SmallStreamInMini) {
    auto path = temp_file("mini_small");
    guard_.add(path);

    auto data = make_test_data(100, 0x11);
    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("Small");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }

    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"Small", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 100u);
    std::vector<uint8_t> buf(100);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 100, &rc)));
    EXPECT_EQ(rc, 100u);
    EXPECT_EQ(buf, data);
}

// ── BelowCutoff: write 4095 bytes ──────────────────────────────────────
TEST_F(MiniStreamConformance, BelowCutoff) {
    auto path = temp_file("mini_below");
    guard_.add(path);

    auto data = make_test_data(4095, 0x22);
    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("Below");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }

    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"Below", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 4095u);
    std::vector<uint8_t> buf(4095);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 4095, &rc)));
    EXPECT_EQ(rc, 4095u);
    EXPECT_EQ(buf, data);
}

// ── ExactCutoff: write exactly 4096 bytes (should use regular FAT) ─────
TEST_F(MiniStreamConformance, ExactCutoff) {
    auto path = temp_file("mini_exact");
    guard_.add(path);

    auto data = make_test_data(4096, 0x33);
    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("Exact");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }

    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"Exact", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 4096u);
    std::vector<uint8_t> buf(4096);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 4096, &rc)));
    EXPECT_EQ(rc, 4096u);
    EXPECT_EQ(buf, data);
}

// ── MultipleMiniStreams: 10 streams of 200 bytes each ───────────────────
TEST_F(MiniStreamConformance, MultipleMiniStreams) {
    auto path = temp_file("mini_multi");
    guard_.add(path);

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        for (int i = 0; i < 10; ++i) {
            auto s = root.create_stream("Mini" + std::to_string(i));
            ASSERT_TRUE(s.has_value());
            auto data = make_test_data(200, static_cast<uint8_t>(i));
            ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        }
        ASSERT_TRUE(cf->flush().has_value());
    }

    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    for (int i = 0; i < 10; ++i) {
        auto name = L"Mini" + std::to_wstring(i);
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(stg->OpenStream(name.c_str(), nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
        EXPECT_EQ(win32_stream_size(strm.get()), 200u);
        std::vector<uint8_t> buf(200);
        ULONG rc = 0;
        ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 200, &rc)));
        auto expected = make_test_data(200, static_cast<uint8_t>(i));
        EXPECT_EQ(buf, expected) << "Mismatch at stream " << i;
    }
}

// ── MiniAndRegularMixed: 5 small + 5 large streams ─────────────────────
TEST_F(MiniStreamConformance, MiniAndRegularMixed) {
    auto path = temp_file("mini_mixed");
    guard_.add(path);

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        for (int i = 0; i < 5; ++i) {
            // Small stream (mini)
            auto s1 = root.create_stream("Small" + std::to_string(i));
            ASSERT_TRUE(s1.has_value());
            auto d1 = make_test_data(100, static_cast<uint8_t>(i));
            ASSERT_TRUE(s1->write(0, std::span<const uint8_t>(d1)).has_value());
            // Large stream (regular)
            auto s2 = root.create_stream("Large" + std::to_string(i));
            ASSERT_TRUE(s2.has_value());
            auto d2 = make_test_data(5000, static_cast<uint8_t>(i + 100));
            ASSERT_TRUE(s2->write(0, std::span<const uint8_t>(d2)).has_value());
        }
        ASSERT_TRUE(cf->flush().has_value());
    }

    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    for (int i = 0; i < 5; ++i) {
        // Check small
        {
            auto name = L"Small" + std::to_wstring(i);
            stream_ptr strm;
            ASSERT_TRUE(
                SUCCEEDED(stg->OpenStream(name.c_str(), nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
            EXPECT_EQ(win32_stream_size(strm.get()), 100u);
        }
        // Check large
        {
            auto name = L"Large" + std::to_wstring(i);
            stream_ptr strm;
            ASSERT_TRUE(
                SUCCEEDED(stg->OpenStream(name.c_str(), nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
            EXPECT_EQ(win32_stream_size(strm.get()), 5000u);
            std::vector<uint8_t> buf(5000);
            ULONG rc = 0;
            ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 5000, &rc)));
            auto expected = make_test_data(5000, static_cast<uint8_t>(i + 100));
            EXPECT_EQ(buf, expected);
        }
    }
}

// ── Win32 writes mini stream, Stout reads ───────────────────────────────
TEST_F(MiniStreamConformance, Win32MiniStoutRead) {
    auto path = temp_file("w32_mini");
    guard_.add(path);

    auto data = make_test_data(500, 0x77);
    {
        storage_ptr stg;
        ASSERT_TRUE(SUCCEEDED(win32_create_v4(path.wstring(), stg.put())));
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(
            stg->CreateStream(L"MiniData", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, strm.put())));
        ASSERT_TRUE(SUCCEEDED(win32_stream_write(strm.get(), data.data(), 500)));
    }

    auto cf = compound_file::open(path, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto s = cf->root_storage().open_stream("MiniData");
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->size(), 500u);
    std::vector<uint8_t> buf(500);
    auto rd = s->read(0, std::span<uint8_t>(buf));
    ASSERT_TRUE(rd.has_value());
    EXPECT_EQ(buf, data);
}

#endif // _WIN32
