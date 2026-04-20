#ifdef _WIN32

#include "conformance_utils.h"
#include <stout/compound_file.h>
#include <gtest/gtest.h>

using namespace conformance;
using namespace stout;

struct VPTimes {
    cfb_version ver;
    uint16_t major;
};

class StressElementTimesConformance : public ::testing::TestWithParam<VPTimes> {
protected:
    com_init com_;
    temp_file_guard guard_;
};

static const VPTimes vp_times[] = {{cfb_version::v3, 3}, {cfb_version::v4, 4}};

INSTANTIATE_TEST_SUITE_P(V, StressElementTimesConformance, ::testing::ValuesIn(vp_times),
    [](const auto& info) { return info.param.major == 3 ? "V3" : "V4"; });

// ── set_element_times on stream ─────────────────────────────────────────

TEST_P(StressElementTimesConformance, SetTimesOnStream) {
    auto p = temp_file("et_strm"); guard_.add(p);
    auto now = std::chrono::system_clock::now();
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("S");
        ASSERT_TRUE(s.has_value());
        auto d = make_test_data(100, 0x11);
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        auto r = cf->root_storage().set_element_times("S", now, now);
        ASSERT_TRUE(r.has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"S", nullptr,
        STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 100u);
}

// ── set_element_times on storage ────────────────────────────────────────

TEST_P(StressElementTimesConformance, SetTimesOnStorage) {
    auto p = temp_file("et_stg"); guard_.add(p);
    auto now = std::chrono::system_clock::now();
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto sub = cf->root_storage().create_storage("Dir");
        ASSERT_TRUE(sub.has_value());
        auto r = cf->root_storage().set_element_times("Dir", now, now);
        ASSERT_TRUE(r.has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    storage_ptr sub;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStorage(L"Dir", nullptr,
        STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0, sub.put())));
}

// ── set_element_times preserves data ────────────────────────────────────

TEST_P(StressElementTimesConformance, SetTimesPreservesData) {
    auto p = temp_file("et_data"); guard_.add(p);
    auto d = make_test_data(500, 0x22);
    auto now = std::chrono::system_clock::now();
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("S");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        ASSERT_TRUE(cf->root_storage().set_element_times("S", now, now).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto s = cf->root_storage().open_stream("S");
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->size(), 500u);
    std::vector<uint8_t> buf(500);
    ASSERT_TRUE(s->read(0, std::span<uint8_t>(buf)).has_value());
    EXPECT_EQ(buf, d);
}

// ── set_element_times on multiple entries ────────────────────────────────

TEST_P(StressElementTimesConformance, SetTimesMultipleEntries) {
    auto p = temp_file("et_multi"); guard_.add(p);
    auto now = std::chrono::system_clock::now();
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        for (int i = 0; i < 5; ++i) {
            auto s = cf->root_storage().create_stream("S" + std::to_string(i));
            ASSERT_TRUE(s.has_value());
            auto d = make_test_data(100, static_cast<uint8_t>(i));
            ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
            ASSERT_TRUE(cf->root_storage().set_element_times("S" + std::to_string(i), now, now).has_value());
        }
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    auto entries = win32_enumerate(stg.get());
    EXPECT_EQ(entries.size(), 5u);
    for (auto& e : entries) free_statstg_name(e);
}

// ── set_element_times on non-existent entry ─────────────────────────────

TEST_P(StressElementTimesConformance, SetTimesNonExistent) {
    auto p = temp_file("et_nf"); guard_.add(p);
    auto cf = compound_file::create(p, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    auto now = std::chrono::system_clock::now();
    auto r = cf->root_storage().set_element_times("Ghost", now, now);
    EXPECT_FALSE(r.has_value());
}

// ── Win32 sets times, Stout reads file ──────────────────────────────────

TEST_P(StressElementTimesConformance, Win32CreatedFileStoutReads) {
    auto p = temp_file("et_w32"); guard_.add(p);
    {
        storage_ptr stg;
        if (GetParam().ver == cfb_version::v4) ASSERT_TRUE(SUCCEEDED(win32_create_v4(p.wstring(), stg.put())));
        else ASSERT_TRUE(SUCCEEDED(win32_create_v3(p.wstring(), stg.put())));
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(stg->CreateStream(L"Data",
            STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, strm.put())));
        auto d = make_test_data(200, 0x33);
        ASSERT_TRUE(SUCCEEDED(win32_stream_write(strm.get(), d.data(), 200)));
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto s = cf->root_storage().open_stream("Data");
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->size(), 200u);
}

// ── Stat times visible via Win32 ────────────────────────────────────────

TEST_P(StressElementTimesConformance, StatTimesVisibleViaWin32) {
    auto p = temp_file("et_stat"); guard_.add(p);
    auto now = std::chrono::system_clock::now();
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("S");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(cf->root_storage().set_element_times("S", now, now).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    auto entries = win32_enumerate(stg.get());
    ASSERT_EQ(entries.size(), 1u);
    // Just verify the entry has a modification time set (non-zero)
    FILETIME zero{};
    bool has_mtime = memcmp(&entries[0].mtime, &zero, sizeof(FILETIME)) != 0;
    EXPECT_TRUE(has_mtime);
    for (auto& e : entries) free_statstg_name(e);
}

#endif // _WIN32
