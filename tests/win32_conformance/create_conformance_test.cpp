#ifdef _WIN32

#include "conformance_utils.h"
#include <stout/compound_file.h>
#include <stout/cfb/header.h>
#include <stout/cfb/constants.h>
#include <gtest/gtest.h>
#include <algorithm>
#include <cstring>

using namespace conformance;
using namespace stout;

class CreateConformance : public ::testing::Test {
protected:
    com_init com_;
    temp_file_guard guard_;
};

// ── EmptyFileV4: Stout creates v4, Win32 can open it ────────────────────
TEST_F(CreateConformance, EmptyFileV4_StoutCreateWin32Open) {
    auto path = temp_file("create_v4");
    guard_.add(path);

    // Stout creates (destructor flushes implicitly via file close)
    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value()) << error_message(cf.error());
        // Note: no explicit flush — create() already writes a valid file
    }

    // Win32 opens
    storage_ptr stg;
    HRESULT hr = win32_open_read(path.wstring(), stg.put());
    ASSERT_TRUE(SUCCEEDED(hr)) << "Win32 failed to open Stout v4 file, hr=0x"
        << std::hex << hr;

    // Verify root entry name
    STATSTG st{};
    hr = stg->Stat(&st, STATFLAG_DEFAULT);
    ASSERT_TRUE(SUCCEEDED(hr));
    // Win32 Stat() on root returns the file path, not "Root Entry"
    EXPECT_TRUE(st.pwcsName != nullptr);
    free_statstg_name(st);
}

// ── EmptyFileV3: Stout creates v3, Win32 can open it ────────────────────
TEST_F(CreateConformance, EmptyFileV3_StoutCreateWin32Open) {
    auto path = temp_file("create_v3");
    guard_.add(path);

    {
        auto cf = compound_file::create(path, cfb_version::v3);
        ASSERT_TRUE(cf.has_value()) << error_message(cf.error());
    }

    storage_ptr stg;
    HRESULT hr = win32_open_read(path.wstring(), stg.put());
    ASSERT_TRUE(SUCCEEDED(hr)) << "Win32 failed to open Stout v3 file, hr=0x"
        << std::hex << hr;

    STATSTG st{};
    hr = stg->Stat(&st, STATFLAG_DEFAULT);
    ASSERT_TRUE(SUCCEEDED(hr));
    EXPECT_TRUE(st.pwcsName != nullptr);
    free_statstg_name(st);
}

// ── Win32 creates v4, Stout can open it ─────────────────────────────────
TEST_F(CreateConformance, EmptyFileV4_Win32CreateStoutOpen) {
    auto path = temp_file("create_w32v4");
    guard_.add(path);

    storage_ptr stg;
    HRESULT hr = win32_create_v4(path.wstring(), stg.put());
    ASSERT_TRUE(SUCCEEDED(hr)) << "Win32 create v4 failed, hr=0x" << std::hex << hr;
    stg.reset();

    auto cf = compound_file::open(path, open_mode::read);
    ASSERT_TRUE(cf.has_value()) << error_message(cf.error());
    EXPECT_EQ(cf->version(), cfb_version::v4);
}

// ── Win32 creates v3, Stout can open it ─────────────────────────────────
TEST_F(CreateConformance, EmptyFileV3_Win32CreateStoutOpen) {
    auto path = temp_file("create_w32v3");
    guard_.add(path);

    storage_ptr stg;
    HRESULT hr = win32_create_v3(path.wstring(), stg.put());
    ASSERT_TRUE(SUCCEEDED(hr)) << "Win32 create v3 failed, hr=0x" << std::hex << hr;
    stg.reset();

    auto cf = compound_file::open(path, open_mode::read);
    ASSERT_TRUE(cf.has_value()) << error_message(cf.error());
    EXPECT_EQ(cf->version(), cfb_version::v3);
}

// ── Header fields match between Stout and Win32 v4 files ────────────────
TEST_F(CreateConformance, HeaderFieldsV4) {
    auto [stout_path, win32_path] = temp_file_pair("hdr_v4");
    guard_.add(stout_path);
    guard_.add(win32_path);

    // Stout creates
    {
        auto cf = compound_file::create(stout_path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }

    // Win32 creates
    {
        storage_ptr stg;
        ASSERT_TRUE(SUCCEEDED(win32_create_v4(win32_path.wstring(), stg.put())));
    }

    // Compare headers
    auto stout_bytes = read_file_bytes(stout_path);
    auto win32_bytes = read_file_bytes(win32_path);
    ASSERT_GE(stout_bytes.size(), 512u);
    ASSERT_GE(win32_bytes.size(), 512u);

    // Magic signature must match (first 8 bytes)
    EXPECT_TRUE(std::equal(stout_bytes.begin(), stout_bytes.begin() + 8,
                           win32_bytes.begin()));

    // Minor version at offset 0x18 (2 bytes) — may differ
    // Major version at offset 0x1A (2 bytes) — must be 4
    uint16_t stout_major = stout_bytes[0x1A] | (stout_bytes[0x1B] << 8);
    uint16_t win32_major = win32_bytes[0x1A] | (win32_bytes[0x1B] << 8);
    EXPECT_EQ(stout_major, 4u);
    EXPECT_EQ(win32_major, 4u);

    // Sector shift at offset 0x1E (2 bytes) — must be 12 for v4
    uint16_t stout_shift = stout_bytes[0x1E] | (stout_bytes[0x1F] << 8);
    uint16_t win32_shift = win32_bytes[0x1E] | (win32_bytes[0x1F] << 8);
    EXPECT_EQ(stout_shift, 12u);
    EXPECT_EQ(win32_shift, 12u);

    // Mini sector shift at offset 0x20 (2 bytes) — must be 6
    uint16_t stout_mini_shift = stout_bytes[0x20] | (stout_bytes[0x21] << 8);
    uint16_t win32_mini_shift = win32_bytes[0x20] | (win32_bytes[0x21] << 8);
    EXPECT_EQ(stout_mini_shift, 6u);
    EXPECT_EQ(win32_mini_shift, 6u);
}

// ── Header fields match between Stout and Win32 v3 files ────────────────
TEST_F(CreateConformance, HeaderFieldsV3) {
    auto [stout_path, win32_path] = temp_file_pair("hdr_v3");
    guard_.add(stout_path);
    guard_.add(win32_path);

    {
        auto cf = compound_file::create(stout_path, cfb_version::v3);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    {
        storage_ptr stg;
        ASSERT_TRUE(SUCCEEDED(win32_create_v3(win32_path.wstring(), stg.put())));
    }

    auto stout_bytes = read_file_bytes(stout_path);
    auto win32_bytes = read_file_bytes(win32_path);
    ASSERT_GE(stout_bytes.size(), 512u);
    ASSERT_GE(win32_bytes.size(), 512u);

    // Major version must be 3
    uint16_t stout_major = stout_bytes[0x1A] | (stout_bytes[0x1B] << 8);
    uint16_t win32_major = win32_bytes[0x1A] | (win32_bytes[0x1B] << 8);
    EXPECT_EQ(stout_major, 3u);
    EXPECT_EQ(win32_major, 3u);

    // Sector shift must be 9 for v3
    uint16_t stout_shift = stout_bytes[0x1E] | (stout_bytes[0x1F] << 8);
    uint16_t win32_shift = win32_bytes[0x1E] | (win32_bytes[0x1F] << 8);
    EXPECT_EQ(stout_shift, 9u);
    EXPECT_EQ(win32_shift, 9u);
}

// ── Root entry CLSID is all-zeros ───────────────────────────────────────
TEST_F(CreateConformance, RootEntryCLSID) {
    auto path = temp_file("root_clsid");
    guard_.add(path);

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }

    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    STATSTG st{};
    ASSERT_TRUE(SUCCEEDED(stg->Stat(&st, STATFLAG_NONAME)));
    EXPECT_EQ(st.clsid, CLSID_NULL);
}

// ── Stout creates, file size is reasonable ──────────────────────────────
TEST_F(CreateConformance, EmptyFileSizeV4) {
    auto path = temp_file("size_v4");
    guard_.add(path);

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }

    auto sz = std::filesystem::file_size(path);
    // v4 file: header (4096) + at least 1 FAT sector + 1 directory sector
    // Minimum is typically 3 * 4096 = 12288 bytes, but could vary
    EXPECT_GE(sz, 4096u) << "v4 file too small";
    // Should be a multiple of sector size (4096)
    EXPECT_EQ(sz % 4096, 0u) << "v4 file size not sector-aligned";
}

TEST_F(CreateConformance, EmptyFileSizeV3) {
    auto path = temp_file("size_v3");
    guard_.add(path);

    {
        auto cf = compound_file::create(path, cfb_version::v3);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }

    auto sz = std::filesystem::file_size(path);
    // v3: header is 512 bytes, sectors are 512 bytes
    EXPECT_GE(sz, 512u) << "v3 file too small";
    EXPECT_EQ(sz % 512, 0u) << "v3 file size not sector-aligned";
}

#endif // _WIN32
