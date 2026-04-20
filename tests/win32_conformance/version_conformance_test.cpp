#ifdef _WIN32

#include "conformance_utils.h"

#include <gtest/gtest.h>
#include <stout/compound_file.h>

using namespace conformance;
using namespace stout;

class VersionConformance : public ::testing::Test {
  protected:
    com_init com_;
    temp_file_guard guard_;
};

// ── V3 Stout → Win32 reads ─────────────────────────────────────────────
TEST_F(VersionConformance, V3StoutV3Win32Read) {
    auto path = temp_file("ver_v3s");
    guard_.add(path);

    auto data = make_test_data(500, 0xAA);
    {
        auto cf = compound_file::create(path, cfb_version::v3);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("Data");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }

    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"Data", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 500u);
    std::vector<uint8_t> buf(500);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 500, &rc)));
    EXPECT_EQ(buf, data);
}

// ── V4 Stout → Win32 reads ─────────────────────────────────────────────
TEST_F(VersionConformance, V4StoutV4Win32Read) {
    auto path = temp_file("ver_v4s");
    guard_.add(path);

    auto data = make_test_data(500, 0xBB);
    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("Data");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }

    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"Data", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 500u);
    std::vector<uint8_t> buf(500);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 500, &rc)));
    EXPECT_EQ(buf, data);
}

// ── V3 Win32 → Stout reads ─────────────────────────────────────────────
TEST_F(VersionConformance, V3Win32V3StoutRead) {
    auto path = temp_file("ver_v3w");
    guard_.add(path);

    auto data = make_test_data(300, 0xCC);
    {
        storage_ptr stg;
        ASSERT_TRUE(SUCCEEDED(win32_create_v3(path.wstring(), stg.put())));
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(
            stg->CreateStream(L"Data", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, strm.put())));
        ASSERT_TRUE(SUCCEEDED(win32_stream_write(strm.get(), data.data(), 300)));
    }

    auto cf = compound_file::open(path, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    EXPECT_EQ(cf->version(), cfb_version::v3);
    auto s = cf->root_storage().open_stream("Data");
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->size(), 300u);
    std::vector<uint8_t> buf(300);
    ASSERT_TRUE(s->read(0, std::span<uint8_t>(buf)).has_value());
    EXPECT_EQ(buf, data);
}

// ── V4 Win32 → Stout reads ─────────────────────────────────────────────
TEST_F(VersionConformance, V4Win32V4StoutRead) {
    auto path = temp_file("ver_v4w");
    guard_.add(path);

    auto data = make_test_data(300, 0xDD);
    {
        storage_ptr stg;
        ASSERT_TRUE(SUCCEEDED(win32_create_v4(path.wstring(), stg.put())));
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(
            stg->CreateStream(L"Data", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, strm.put())));
        ASSERT_TRUE(SUCCEEDED(win32_stream_write(strm.get(), data.data(), 300)));
    }

    auto cf = compound_file::open(path, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    EXPECT_EQ(cf->version(), cfb_version::v4);
    auto s = cf->root_storage().open_stream("Data");
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->size(), 300u);
    std::vector<uint8_t> buf(300);
    ASSERT_TRUE(s->read(0, std::span<uint8_t>(buf)).has_value());
    EXPECT_EQ(buf, data);
}

// ── V3 header signature valid ───────────────────────────────────────────
TEST_F(VersionConformance, V3HeaderSignature) {
    auto path = temp_file("ver_v3sig");
    guard_.add(path);

    {
        auto cf = compound_file::create(path, cfb_version::v3);
        ASSERT_TRUE(cf.has_value());
    }

    auto bytes = read_file_bytes(path);
    ASSERT_GE(bytes.size(), 8u);
    // CFB magic: D0 CF 11 E0 A1 B1 1A E1
    EXPECT_EQ(bytes[0], 0xD0);
    EXPECT_EQ(bytes[1], 0xCF);
    EXPECT_EQ(bytes[2], 0x11);
    EXPECT_EQ(bytes[3], 0xE0);
    EXPECT_EQ(bytes[4], 0xA1);
    EXPECT_EQ(bytes[5], 0xB1);
    EXPECT_EQ(bytes[6], 0x1A);
    EXPECT_EQ(bytes[7], 0xE1);

    // Minor version at offset 0x18, major at 0x1A
    uint16_t major = bytes[0x1A] | (bytes[0x1B] << 8);
    EXPECT_EQ(major, 3u);
}

// ── V4 header signature valid ───────────────────────────────────────────
TEST_F(VersionConformance, V4HeaderSignature) {
    auto path = temp_file("ver_v4sig");
    guard_.add(path);

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
    }

    auto bytes = read_file_bytes(path);
    ASSERT_GE(bytes.size(), 8u);
    EXPECT_EQ(bytes[0], 0xD0);
    EXPECT_EQ(bytes[1], 0xCF);
    EXPECT_EQ(bytes[2], 0x11);
    EXPECT_EQ(bytes[3], 0xE0);
    EXPECT_EQ(bytes[4], 0xA1);
    EXPECT_EQ(bytes[5], 0xB1);
    EXPECT_EQ(bytes[6], 0x1A);
    EXPECT_EQ(bytes[7], 0xE1);

    uint16_t major = bytes[0x1A] | (bytes[0x1B] << 8);
    EXPECT_EQ(major, 4u);
}

#endif // _WIN32
