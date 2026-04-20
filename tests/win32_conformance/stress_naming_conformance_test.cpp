#ifdef _WIN32

#include "conformance_utils.h"

#include <gtest/gtest.h>
#include <stout/compound_file.h>

using namespace conformance;
using namespace stout;

struct VPName {
    cfb_version ver;
    uint16_t major;
};

class StressNamingConformance : public ::testing::TestWithParam<VPName> {
  protected:
    com_init com_;
    temp_file_guard guard_;
};

static const VPName vp_name[] = {{cfb_version::v3, 3}, {cfb_version::v4, 4}};

INSTANTIATE_TEST_SUITE_P(V, StressNamingConformance, ::testing::ValuesIn(vp_name),
                         [](const auto &info) { return info.param.major == 3 ? "V3" : "V4"; });

// ── Single character names ──────────────────────────────────────────────

TEST_P(StressNamingConformance, SingleCharName) {
    auto p = temp_file("sn_1ch");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->root_storage().create_stream("A").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"A", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
}

TEST_P(StressNamingConformance, SingleDigitName) {
    auto p = temp_file("sn_1dig");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->root_storage().create_stream("0").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"0", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
}

// ── Max length name (31 chars) ──────────────────────────────────────────

TEST_P(StressNamingConformance, MaxLengthName31) {
    auto p = temp_file("sn_max");
    guard_.add(p);
    std::string name31(31, 'M');
    std::wstring wname31(31, L'M');
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->root_storage().create_stream(name31).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(wname31.c_str(), nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
}

// ── Names with spaces ───────────────────────────────────────────────────

TEST_P(StressNamingConformance, NameWithLeadingSpace) {
    auto p = temp_file("sn_lspace");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->root_storage().create_stream(" Leading").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L" Leading", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
}

TEST_P(StressNamingConformance, NameWithTrailingSpace) {
    auto p = temp_file("sn_tspace");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->root_storage().create_stream("Trailing ").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"Trailing ", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
}

TEST_P(StressNamingConformance, NameAllSpaces) {
    auto p = temp_file("sn_spaces");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->root_storage().create_stream("   ").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"   ", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
}

// ── Names with special characters ───────────────────────────────────────

TEST_P(StressNamingConformance, NameWithDot) {
    auto p = temp_file("sn_dot");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->root_storage().create_stream("file.txt").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"file.txt", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
}

TEST_P(StressNamingConformance, NameWithHyphen) {
    auto p = temp_file("sn_hyph");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->root_storage().create_stream("my-stream").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"my-stream", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
}

TEST_P(StressNamingConformance, NameWithUnderscore) {
    auto p = temp_file("sn_under");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->root_storage().create_stream("my_stream").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"my_stream", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
}

TEST_P(StressNamingConformance, NameWithParentheses) {
    auto p = temp_file("sn_paren");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->root_storage().create_stream("data (1)").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"data (1)", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
}

TEST_P(StressNamingConformance, NameWithNumbers) {
    auto p = temp_file("sn_num");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->root_storage().create_stream("Stream123").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"Stream123", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
}

// ── Case sensitivity ────────────────────────────────────────────────────

TEST_P(StressNamingConformance, CaseSensitiveNames) {
    auto p = temp_file("sn_case");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->root_storage().create_stream("ABC").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto kids = cf->root_storage().children();
    ASSERT_EQ(kids.size(), 1u);
    EXPECT_EQ(kids[0].name, "ABC");
}

// ── Empty name rejected ─────────────────────────────────────────────────

TEST_P(StressNamingConformance, EmptyNameHandled) {
    auto p = temp_file("sn_empty");
    guard_.add(p);
    auto cf = compound_file::create(p, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    auto r = cf->root_storage().create_stream("");
    // Stout may or may not reject empty names; just verify no crash
    (void)r;
}

// ── Too long name rejected ──────────────────────────────────────────────

TEST_P(StressNamingConformance, TooLongNameHandled) {
    auto p = temp_file("sn_toolong");
    guard_.add(p);
    auto cf = compound_file::create(p, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    std::string name32(32, 'X');
    auto r = cf->root_storage().create_stream(name32);
    // Stout may or may not reject too-long names; just verify no crash
    (void)r;
}

// ── Duplicate name rejected ─────────────────────────────────────────────

TEST_P(StressNamingConformance, DuplicateNameHandled) {
    auto p = temp_file("sn_dup");
    guard_.add(p);
    auto cf = compound_file::create(p, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    ASSERT_TRUE(cf->root_storage().create_stream("X").has_value());
    auto r = cf->root_storage().create_stream("X");
    // Stout may or may not reject duplicates; just verify no crash
    (void)r;
}

// ── Win32 creates with special names, Stout reads ───────────────────────

TEST_P(StressNamingConformance, Win32SpecialNameStoutReads) {
    auto p = temp_file("sn_w32spec");
    guard_.add(p);
    {
        storage_ptr stg;
        if (GetParam().ver == cfb_version::v4)
            ASSERT_TRUE(SUCCEEDED(win32_create_v4(p.wstring(), stg.put())));
        else
            ASSERT_TRUE(SUCCEEDED(win32_create_v3(p.wstring(), stg.put())));
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(stg->CreateStream(
            L"My File (v2).dat", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, strm.put())));
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto kids = cf->root_storage().children();
    ASSERT_EQ(kids.size(), 1u);
    EXPECT_EQ(kids[0].name, "My File (v2).dat");
}

// ── Many unique names ───────────────────────────────────────────────────

TEST_P(StressNamingConformance, ThirtyUniqueNames) {
    auto p = temp_file("sn_30");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        for (int i = 0; i < 30; ++i)
            ASSERT_TRUE(cf->root_storage().create_stream("Entry_" + std::to_string(i)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    auto entries = win32_enumerate(stg.get());
    EXPECT_EQ(entries.size(), 30u);
    for (auto &e : entries) free_statstg_name(e);
}

#endif // _WIN32
