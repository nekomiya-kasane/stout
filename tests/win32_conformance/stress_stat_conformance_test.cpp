#ifdef _WIN32

#include "conformance_utils.h"

#include <gtest/gtest.h>
#include <stout/compound_file.h>

using namespace conformance;
using namespace stout;

struct VPStat {
    cfb_version ver;
    uint16_t major;
};

class StressStatConformance : public ::testing::TestWithParam<VPStat> {
  protected:
    com_init com_;
    temp_file_guard guard_;
};

static const VPStat vp_stat[] = {{cfb_version::v3, 3}, {cfb_version::v4, 4}};

INSTANTIATE_TEST_SUITE_P(V, StressStatConformance, ::testing::ValuesIn(vp_stat),
                         [](const auto &info) { return info.param.major == 3 ? "V3" : "V4"; });

// ── Root storage stat ───────────────────────────────────────────────────

TEST_P(StressStatConformance, RootStorageStat) {
    auto p = temp_file("st_root");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->root_storage().create_stream("A").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    STATSTG stat{};
    ASSERT_TRUE(SUCCEEDED(stg->Stat(&stat, STATFLAG_DEFAULT)));
    EXPECT_EQ(stat.type, static_cast<DWORD>(STGTY_STORAGE));
    if (stat.pwcsName) CoTaskMemFree(stat.pwcsName);
}

// ── Stream stat size matches ────────────────────────────────────────────

TEST_P(StressStatConformance, StreamStatSizeMatches) {
    auto p = temp_file("st_size");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("S");
        ASSERT_TRUE(s.has_value());
        auto d = make_test_data(777, 0x11);
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"S", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    STATSTG stat{};
    ASSERT_TRUE(SUCCEEDED(strm->Stat(&stat, STATFLAG_NONAME)));
    EXPECT_EQ(stat.cbSize.QuadPart, 777u);
    EXPECT_EQ(stat.type, static_cast<DWORD>(STGTY_STREAM));
}

// ── Empty stream stat ───────────────────────────────────────────────────

TEST_P(StressStatConformance, EmptyStreamStat) {
    auto p = temp_file("st_empty");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->root_storage().create_stream("E").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"E", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    STATSTG stat{};
    ASSERT_TRUE(SUCCEEDED(strm->Stat(&stat, STATFLAG_NONAME)));
    EXPECT_EQ(stat.cbSize.QuadPart, 0u);
}

// ── Sub-storage stat type ───────────────────────────────────────────────

TEST_P(StressStatConformance, SubStorageStatType) {
    auto p = temp_file("st_substg");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->root_storage().create_storage("Dir").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    storage_ptr sub;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStorage(L"Dir", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0, sub.put())));
    STATSTG stat{};
    ASSERT_TRUE(SUCCEEDED(sub->Stat(&stat, STATFLAG_DEFAULT)));
    EXPECT_EQ(stat.type, static_cast<DWORD>(STGTY_STORAGE));
    if (stat.pwcsName) {
        EXPECT_STREQ(stat.pwcsName, L"Dir");
        CoTaskMemFree(stat.pwcsName);
    }
}

// ── Enumerate names match Stout children ────────────────────────────────

TEST_P(StressStatConformance, EnumerateNamesMatch) {
    auto p = temp_file("st_names");
    guard_.add(p);
    std::vector<std::string> names = {"Alpha", "Beta", "Gamma", "Delta", "Epsilon"};
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        for (auto &n : names) {
            auto s = cf->root_storage().create_stream(n);
            ASSERT_TRUE(s.has_value());
        }
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    auto entries = win32_enumerate(stg.get());
    ASSERT_EQ(entries.size(), 5u);
    std::set<std::wstring> win32_names;
    for (auto &e : entries) {
        if (e.pwcsName) win32_names.insert(e.pwcsName);
        free_statstg_name(e);
    }
    for (auto &n : names) {
        std::wstring wn(n.begin(), n.end());
        EXPECT_TRUE(win32_names.count(wn) > 0) << "Missing: " << n;
    }
}

// ── Enumerate types match ───────────────────────────────────────────────

TEST_P(StressStatConformance, EnumerateTypesMatch) {
    auto p = temp_file("st_types");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->root_storage().create_stream("S1").has_value());
        ASSERT_TRUE(cf->root_storage().create_storage("D1").has_value());
        ASSERT_TRUE(cf->root_storage().create_stream("S2").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    auto entries = win32_enumerate(stg.get());
    ASSERT_EQ(entries.size(), 3u);
    int streams = 0, storages = 0;
    for (auto &e : entries) {
        if (e.type == STGTY_STREAM)
            ++streams;
        else if (e.type == STGTY_STORAGE)
            ++storages;
        free_statstg_name(e);
    }
    EXPECT_EQ(streams, 2);
    EXPECT_EQ(storages, 1);
}

// ── Enumerate sizes match ───────────────────────────────────────────────

TEST_P(StressStatConformance, EnumerateSizesMatch) {
    auto p = temp_file("st_sizes");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        for (int i = 0; i < 5; ++i) {
            auto s = cf->root_storage().create_stream("S" + std::to_string(i));
            ASSERT_TRUE(s.has_value());
            auto d = make_test_data(100 * (i + 1), static_cast<uint8_t>(i));
            ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        }
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    for (int i = 0; i < 5; ++i) {
        auto name = L"S" + std::to_wstring(i);
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(stg->OpenStream(name.c_str(), nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
        STATSTG stat{};
        ASSERT_TRUE(SUCCEEDED(strm->Stat(&stat, STATFLAG_NONAME)));
        EXPECT_EQ(stat.cbSize.QuadPart, static_cast<uint64_t>(100 * (i + 1)));
    }
}

// ── Stout children match Win32 enumerate count ──────────────────────────

TEST_P(StressStatConformance, ChildrenCountMatch) {
    auto p = temp_file("st_count");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        for (int i = 0; i < 8; ++i) ASSERT_TRUE(cf->root_storage().create_stream("S" + std::to_string(i)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    // Stout count
    size_t stout_count;
    {
        auto cf = compound_file::open(p, open_mode::read);
        ASSERT_TRUE(cf.has_value());
        stout_count = cf->root_storage().children().size();
    }
    // Win32 count
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    auto entries = win32_enumerate(stg.get());
    EXPECT_EQ(entries.size(), stout_count);
    for (auto &e : entries) free_statstg_name(e);
}

// ── Stout stream size matches Win32 stat ────────────────────────────────

TEST_P(StressStatConformance, StoutSizeMatchesWin32Stat) {
    auto p = temp_file("st_szmatch");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("S");
        ASSERT_TRUE(s.has_value());
        auto d = make_test_data(3333, 0x22);
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    uint64_t stout_size;
    {
        auto cf = compound_file::open(p, open_mode::read);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().open_stream("S");
        ASSERT_TRUE(s.has_value());
        stout_size = s->size();
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"S", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), stout_size);
}

#endif // _WIN32
