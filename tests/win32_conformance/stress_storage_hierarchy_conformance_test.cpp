#ifdef _WIN32

#include "conformance_utils.h"

#include <gtest/gtest.h>
#include <set>
#include <stout/compound_file.h>

using namespace conformance;
using namespace stout;

struct VersionParam5 {
    cfb_version ver;
    uint16_t major;
};

class StressStorageHierarchyConformance : public ::testing::TestWithParam<VersionParam5> {
  protected:
    com_init com_;
    temp_file_guard guard_;
};

static const VersionParam5 versions5[] = {
    {cfb_version::v3, 3},
    {cfb_version::v4, 4},
};

INSTANTIATE_TEST_SUITE_P(V, StressStorageHierarchyConformance, ::testing::ValuesIn(versions5),
                         [](const auto &info) { return info.param.major == 3 ? "V3" : "V4"; });

// ── Variable number of sub-storages ─────────────────────────────────────

TEST_P(StressStorageHierarchyConformance, OneSubStorage) {
    auto p = temp_file("sh_1");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->root_storage().create_storage("S0").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    auto entries = win32_enumerate(stg.get());
    EXPECT_EQ(entries.size(), 1u);
    for (auto &e : entries) free_statstg_name(e);
}

TEST_P(StressStorageHierarchyConformance, FiveSubStorages) {
    auto p = temp_file("sh_5");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        for (int i = 0; i < 5; ++i) ASSERT_TRUE(cf->root_storage().create_storage("S" + std::to_string(i)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    EXPECT_EQ(win32_enumerate(stg.get()).size(), 5u);
}

TEST_P(StressStorageHierarchyConformance, TwentySubStorages) {
    auto p = temp_file("sh_20");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        for (int i = 0; i < 20; ++i)
            ASSERT_TRUE(cf->root_storage().create_storage("S" + std::to_string(i)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    auto entries = win32_enumerate(stg.get());
    EXPECT_EQ(entries.size(), 20u);
    for (auto &e : entries) free_statstg_name(e);
}

TEST_P(StressStorageHierarchyConformance, FiftySubStorages) {
    auto p = temp_file("sh_50");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        for (int i = 0; i < 50; ++i)
            ASSERT_TRUE(cf->root_storage().create_storage("S" + std::to_string(i)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    EXPECT_EQ(win32_enumerate(stg.get()).size(), 50u);
}

// ── Nested levels ───────────────────────────────────────────────────────

TEST_P(StressStorageHierarchyConformance, Nested2Levels) {
    auto p = temp_file("sh_n2");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto a = cf->root_storage().create_storage("A");
        ASSERT_TRUE(a.has_value());
        ASSERT_TRUE(a->create_storage("B").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    storage_ptr a;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStorage(L"A", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0, a.put())));
    storage_ptr b;
    ASSERT_TRUE(SUCCEEDED(a->OpenStorage(L"B", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0, b.put())));
}

TEST_P(StressStorageHierarchyConformance, Nested5Levels) {
    auto p = temp_file("sh_n5");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        storage cur = cf->root_storage();
        for (int i = 0; i < 5; ++i) {
            auto sub = cur.create_storage("L" + std::to_string(i));
            ASSERT_TRUE(sub.has_value());
            cur = std::move(*sub);
        }
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    IStorage *cur = stg.get();
    std::vector<storage_ptr> holders;
    for (int i = 0; i < 5; ++i) {
        auto name = L"L" + std::to_wstring(i);
        holders.emplace_back();
        ASSERT_TRUE(SUCCEEDED(cur->OpenStorage(name.c_str(), nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0,
                                               holders.back().put())));
        cur = holders.back().get();
    }
}

TEST_P(StressStorageHierarchyConformance, Nested10Levels) {
    auto p = temp_file("sh_n10");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        storage cur = cf->root_storage();
        for (int i = 0; i < 10; ++i) {
            auto sub = cur.create_storage("L" + std::to_string(i));
            ASSERT_TRUE(sub.has_value());
            cur = std::move(*sub);
        }
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    IStorage *cur = stg.get();
    std::vector<storage_ptr> holders;
    for (int i = 0; i < 10; ++i) {
        auto name = L"L" + std::to_wstring(i);
        holders.emplace_back();
        ASSERT_TRUE(SUCCEEDED(cur->OpenStorage(name.c_str(), nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0,
                                               holders.back().put())));
        cur = holders.back().get();
    }
}

TEST_P(StressStorageHierarchyConformance, Nested20Levels) {
    auto p = temp_file("sh_n20");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        storage cur = cf->root_storage();
        for (int i = 0; i < 20; ++i) {
            auto sub = cur.create_storage("L" + std::to_string(i));
            ASSERT_TRUE(sub.has_value());
            cur = std::move(*sub);
        }
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    IStorage *cur = stg.get();
    std::vector<storage_ptr> holders;
    for (int i = 0; i < 20; ++i) {
        auto name = L"L" + std::to_wstring(i);
        holders.emplace_back();
        ASSERT_TRUE(SUCCEEDED(cur->OpenStorage(name.c_str(), nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0,
                                               holders.back().put())));
        cur = holders.back().get();
    }
}

// ── Each level with streams ─────────────────────────────────────────────

TEST_P(StressStorageHierarchyConformance, EachLevelWithStream) {
    auto p = temp_file("sh_lvlstrm");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        storage cur = cf->root_storage();
        for (int i = 0; i < 5; ++i) {
            auto s = cur.create_stream("Data" + std::to_string(i));
            ASSERT_TRUE(s.has_value());
            auto data = make_test_data(100, static_cast<uint8_t>(i));
            ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
            auto sub = cur.create_storage("Level" + std::to_string(i));
            ASSERT_TRUE(sub.has_value());
            cur = std::move(*sub);
        }
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    IStorage *cur = stg.get();
    std::vector<storage_ptr> holders;
    for (int i = 0; i < 5; ++i) {
        auto sname = L"Data" + std::to_wstring(i);
        stream_ptr strm;
        ASSERT_TRUE(
            SUCCEEDED(cur->OpenStream(sname.c_str(), nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
        EXPECT_EQ(win32_stream_size(strm.get()), 100u);
        auto lname = L"Level" + std::to_wstring(i);
        holders.emplace_back();
        ASSERT_TRUE(SUCCEEDED(cur->OpenStorage(lname.c_str(), nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0,
                                               holders.back().put())));
        cur = holders.back().get();
    }
}

// ── Wide: 100 siblings ──────────────────────────────────────────────────

TEST_P(StressStorageHierarchyConformance, HundredSiblings) {
    auto p = temp_file("sh_100");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        for (int i = 0; i < 100; ++i) ASSERT_TRUE(root.create_stream("S" + std::to_string(i)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    EXPECT_EQ(win32_enumerate(stg.get()).size(), 100u);
}

// ── Win32 creates deep hierarchy, Stout traverses ───────────────────────

TEST_P(StressStorageHierarchyConformance, Win32DeepStoutTraverses) {
    auto p = temp_file("sh_w32deep");
    guard_.add(p);
    {
        storage_ptr stg;
        if (GetParam().ver == cfb_version::v4)
            ASSERT_TRUE(SUCCEEDED(win32_create_v4(p.wstring(), stg.put())));
        else
            ASSERT_TRUE(SUCCEEDED(win32_create_v3(p.wstring(), stg.put())));
        IStorage *cur = stg.get();
        std::vector<storage_ptr> holders;
        for (int i = 0; i < 5; ++i) {
            auto name = L"D" + std::to_wstring(i);
            holders.emplace_back();
            ASSERT_TRUE(SUCCEEDED(cur->CreateStorage(name.c_str(), STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE,
                                                     0, 0, holders.back().put())));
            cur = holders.back().get();
        }
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    storage cur = cf->root_storage();
    for (int i = 0; i < 5; ++i) {
        auto sub = cur.open_storage("D" + std::to_string(i));
        ASSERT_TRUE(sub.has_value()) << "Failed at D" << i;
        cur = std::move(*sub);
    }
}

// ── Stout creates deep, Win32 traverses ─────────────────────────────────

TEST_P(StressStorageHierarchyConformance, StoutDeepWin32Traverses) {
    auto p = temp_file("sh_sdeep");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        storage cur = cf->root_storage();
        for (int i = 0; i < 8; ++i) {
            auto sub = cur.create_storage("N" + std::to_string(i));
            ASSERT_TRUE(sub.has_value());
            cur = std::move(*sub);
        }
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    IStorage *cur = stg.get();
    std::vector<storage_ptr> holders;
    for (int i = 0; i < 8; ++i) {
        auto name = L"N" + std::to_wstring(i);
        holders.emplace_back();
        ASSERT_TRUE(SUCCEEDED(cur->OpenStorage(name.c_str(), nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0,
                                               holders.back().put())));
        cur = holders.back().get();
    }
}

// ── Empty sub-storage ───────────────────────────────────────────────────

TEST_P(StressStorageHierarchyConformance, EmptySubStorage) {
    auto p = temp_file("sh_empty");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->root_storage().create_storage("Empty").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    storage_ptr sub;
    ASSERT_TRUE(
        SUCCEEDED(stg->OpenStorage(L"Empty", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0, sub.put())));
    EXPECT_EQ(win32_enumerate(sub.get()).size(), 0u);
}

// ── Sub-storage with empty stream ───────────────────────────────────────

TEST_P(StressStorageHierarchyConformance, SubStorageWithEmptyStream) {
    auto p = temp_file("sh_estrm");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto sub = cf->root_storage().create_storage("Sub");
        ASSERT_TRUE(sub.has_value());
        ASSERT_TRUE(sub->create_stream("Empty").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    storage_ptr sub;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStorage(L"Sub", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0, sub.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(sub->OpenStream(L"Empty", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 0u);
}

// ── Mixed: storages only, streams only, both ────────────────────────────

TEST_P(StressStorageHierarchyConformance, StoragesOnly) {
    auto p = temp_file("sh_stgonly");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        for (int i = 0; i < 10; ++i) ASSERT_TRUE(root.create_storage("D" + std::to_string(i)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    auto entries = win32_enumerate(stg.get());
    EXPECT_EQ(entries.size(), 10u);
    for (auto &e : entries) {
        EXPECT_EQ(e.type, STGTY_STORAGE);
        free_statstg_name(e);
    }
}

TEST_P(StressStorageHierarchyConformance, StreamsOnly) {
    auto p = temp_file("sh_strmonly");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        for (int i = 0; i < 10; ++i) {
            auto s = root.create_stream("F" + std::to_string(i));
            ASSERT_TRUE(s.has_value());
            auto data = make_test_data(50, static_cast<uint8_t>(i));
            ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        }
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    auto entries = win32_enumerate(stg.get());
    EXPECT_EQ(entries.size(), 10u);
    for (auto &e : entries) {
        EXPECT_EQ(e.type, STGTY_STREAM);
        free_statstg_name(e);
    }
}

TEST_P(StressStorageHierarchyConformance, MixedStoragesAndStreams) {
    auto p = temp_file("sh_mixed");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        for (int i = 0; i < 5; ++i) {
            ASSERT_TRUE(root.create_storage("D" + std::to_string(i)).has_value());
            auto s = root.create_stream("F" + std::to_string(i));
            ASSERT_TRUE(s.has_value());
        }
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    auto entries = win32_enumerate(stg.get());
    EXPECT_EQ(entries.size(), 10u);
    int stg_count = 0, strm_count = 0;
    for (auto &e : entries) {
        if (e.type == STGTY_STORAGE)
            ++stg_count;
        else if (e.type == STGTY_STREAM)
            ++strm_count;
        free_statstg_name(e);
    }
    EXPECT_EQ(stg_count, 5);
    EXPECT_EQ(strm_count, 5);
}

// ── Storage name with special chars ─────────────────────────────────────

TEST_P(StressStorageHierarchyConformance, NameWithSpaces) {
    auto p = temp_file("sh_spaces");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->root_storage().create_storage("My Storage").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    storage_ptr sub;
    ASSERT_TRUE(
        SUCCEEDED(stg->OpenStorage(L"My Storage", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0, sub.put())));
}

TEST_P(StressStorageHierarchyConformance, NameWithDots) {
    auto p = temp_file("sh_dots");
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

TEST_P(StressStorageHierarchyConformance, NameWithHyphens) {
    auto p = temp_file("sh_hyph");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->root_storage().create_storage("my-storage").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    storage_ptr sub;
    ASSERT_TRUE(
        SUCCEEDED(stg->OpenStorage(L"my-storage", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0, sub.put())));
}

#endif // _WIN32
