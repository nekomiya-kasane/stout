#ifdef _WIN32

#include "conformance_utils.h"

#include <algorithm>
#include <gtest/gtest.h>
#include <set>
#include <stout/compound_file.h>

using namespace conformance;
using namespace stout;

struct VPEnum {
    cfb_version ver;
    uint16_t major;
};

class StressEnumerationConformance : public ::testing::TestWithParam<VPEnum> {
  protected:
    com_init com_;
    temp_file_guard guard_;
};

static const VPEnum vp_enum[] = {
    {cfb_version::v3, 3},
    {cfb_version::v4, 4},
};

INSTANTIATE_TEST_SUITE_P(V, StressEnumerationConformance, ::testing::ValuesIn(vp_enum),
                         [](const auto &info) { return info.param.major == 3 ? "V3" : "V4"; });

// ── Count-based enumeration ─────────────────────────────────────────────

TEST_P(StressEnumerationConformance, ZeroChildren) {
    auto p = temp_file("se_0");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    {
        auto cf = compound_file::open(p, open_mode::read);
        ASSERT_TRUE(cf.has_value());
        EXPECT_EQ(cf->root_storage().children().size(), 0u);
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    EXPECT_EQ(win32_enumerate(stg.get()).size(), 0u);
}

TEST_P(StressEnumerationConformance, OneChild) {
    auto p = temp_file("se_1");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->root_storage().create_stream("A").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    {
        auto cf = compound_file::open(p, open_mode::read);
        ASSERT_TRUE(cf.has_value());
        EXPECT_EQ(cf->root_storage().children().size(), 1u);
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    EXPECT_EQ(win32_enumerate(stg.get()).size(), 1u);
}

TEST_P(StressEnumerationConformance, FiveChildren) {
    auto p = temp_file("se_5");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        for (int i = 0; i < 5; ++i) {
            ASSERT_TRUE(cf->root_storage().create_stream("S" + std::to_string(i)).has_value());
        }
        ASSERT_TRUE(cf->flush().has_value());
    }
    {
        auto cf = compound_file::open(p, open_mode::read);
        ASSERT_TRUE(cf.has_value());
        EXPECT_EQ(cf->root_storage().children().size(), 5u);
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    EXPECT_EQ(win32_enumerate(stg.get()).size(), 5u);
}

TEST_P(StressEnumerationConformance, TwentyChildren) {
    auto p = temp_file("se_20");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        for (int i = 0; i < 20; ++i) {
            ASSERT_TRUE(cf->root_storage().create_stream("S" + std::to_string(i)).has_value());
        }
        ASSERT_TRUE(cf->flush().has_value());
    }
    {
        auto cf = compound_file::open(p, open_mode::read);
        ASSERT_TRUE(cf.has_value());
        EXPECT_EQ(cf->root_storage().children().size(), 20u);
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    EXPECT_EQ(win32_enumerate(stg.get()).size(), 20u);
}

TEST_P(StressEnumerationConformance, FiftyChildren) {
    auto p = temp_file("se_50");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        for (int i = 0; i < 50; ++i) {
            ASSERT_TRUE(cf->root_storage().create_stream("S" + std::to_string(i)).has_value());
        }
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    EXPECT_EQ(cf->root_storage().children().size(), 50u);
}

// ── Name matching ───────────────────────────────────────────────────────

TEST_P(StressEnumerationConformance, NamesMatchBetweenAPIs) {
    auto p = temp_file("se_names");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        ASSERT_TRUE(root.create_stream("Alpha").has_value());
        ASSERT_TRUE(root.create_storage("Beta").has_value());
        ASSERT_TRUE(root.create_stream("Gamma").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    // Stout names
    {
        auto cf = compound_file::open(p, open_mode::read);
        ASSERT_TRUE(cf.has_value());
        auto stout_kids = cf->root_storage().children();
        std::set<std::string> stout_names;
        for (auto &c : stout_kids) {
            stout_names.insert(c.name);
        }
        EXPECT_TRUE(stout_names.count("Alpha"));
        EXPECT_TRUE(stout_names.count("Beta"));
        EXPECT_TRUE(stout_names.count("Gamma"));
    }
    // Win32 names
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    auto w32_kids = win32_enumerate(stg.get());
    std::set<std::wstring> w32_names;
    for (auto &e : w32_kids) {
        w32_names.insert(e.pwcsName);
        free_statstg_name(e);
    }
    EXPECT_TRUE(w32_names.count(L"Alpha"));
    EXPECT_TRUE(w32_names.count(L"Beta"));
    EXPECT_TRUE(w32_names.count(L"Gamma"));
}

// ── Type matching ───────────────────────────────────────────────────────

TEST_P(StressEnumerationConformance, TypesMatchBetweenAPIs) {
    auto p = temp_file("se_types");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        ASSERT_TRUE(root.create_stream("File").has_value());
        ASSERT_TRUE(root.create_storage("Dir").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    {
        auto cf = compound_file::open(p, open_mode::read);
        ASSERT_TRUE(cf.has_value());
        auto kids = cf->root_storage().children();
        for (auto &c : kids) {
            if (c.name == "File") {
                EXPECT_EQ(c.type, entry_type::stream);
            }
            if (c.name == "Dir") {
                EXPECT_EQ(c.type, entry_type::storage);
            }
        }
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    auto w32 = win32_enumerate(stg.get());
    for (auto &e : w32) {
        if (std::wstring(e.pwcsName) == L"File") {
            EXPECT_EQ(e.type, STGTY_STREAM);
        }
        if (std::wstring(e.pwcsName) == L"Dir") {
            EXPECT_EQ(e.type, STGTY_STORAGE);
        }
        free_statstg_name(e);
    }
}

// ── Size matching ───────────────────────────────────────────────────────

TEST_P(StressEnumerationConformance, SizesMatchBetweenAPIs) {
    auto p = temp_file("se_sizes");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        auto s1 = root.create_stream("Small");
        ASSERT_TRUE(s1.has_value());
        auto d1 = make_test_data(100, 0x11);
        ASSERT_TRUE(s1->write(0, std::span<const uint8_t>(d1)).has_value());
        auto s2 = root.create_stream("Big");
        ASSERT_TRUE(s2.has_value());
        auto d2 = make_test_data(5000, 0x22);
        ASSERT_TRUE(s2->write(0, std::span<const uint8_t>(d2)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    {
        auto cf = compound_file::open(p, open_mode::read);
        ASSERT_TRUE(cf.has_value());
        auto kids = cf->root_storage().children();
        for (auto &c : kids) {
            if (c.name == "Small") {
                EXPECT_EQ(c.size, 100u);
            }
            if (c.name == "Big") {
                EXPECT_EQ(c.size, 5000u);
            }
        }
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    auto w32 = win32_enumerate(stg.get());
    for (auto &e : w32) {
        if (std::wstring(e.pwcsName) == L"Small") {
            EXPECT_EQ(e.cbSize.QuadPart, 100u);
        }
        if (std::wstring(e.pwcsName) == L"Big") {
            EXPECT_EQ(e.cbSize.QuadPart, 5000u);
        }
        free_statstg_name(e);
    }
}

// ── Enumerate after add/delete ──────────────────────────────────────────

TEST_P(StressEnumerationConformance, EnumerateAfterDelete) {
    auto p = temp_file("se_del");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        ASSERT_TRUE(root.create_stream("A").has_value());
        ASSERT_TRUE(root.create_stream("B").has_value());
        ASSERT_TRUE(root.create_stream("C").has_value());
        ASSERT_TRUE(root.remove("B").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto kids = cf->root_storage().children();
    EXPECT_EQ(kids.size(), 2u);
    std::set<std::string> names;
    for (auto &c : kids) {
        names.insert(c.name);
    }
    EXPECT_TRUE(names.count("A"));
    EXPECT_TRUE(names.count("C"));
    EXPECT_FALSE(names.count("B"));
}

TEST_P(StressEnumerationConformance, EnumerateAfterAddAndDelete) {
    auto p = temp_file("se_adddel");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        for (int i = 0; i < 10; ++i) {
            ASSERT_TRUE(root.create_stream("S" + std::to_string(i)).has_value());
        }
        ASSERT_TRUE(root.remove("S3").has_value());
        ASSERT_TRUE(root.remove("S7").has_value());
        ASSERT_TRUE(root.create_stream("New").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto kids = cf->root_storage().children();
    EXPECT_EQ(kids.size(), 9u);
}

// ── Enumerate sub-storage children ──────────────────────────────────────

TEST_P(StressEnumerationConformance, SubStorageChildren) {
    auto p = temp_file("se_sub");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto sub = cf->root_storage().create_storage("Sub");
        ASSERT_TRUE(sub.has_value());
        ASSERT_TRUE(sub->create_stream("X").has_value());
        ASSERT_TRUE(sub->create_stream("Y").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    {
        auto cf = compound_file::open(p, open_mode::read);
        ASSERT_TRUE(cf.has_value());
        auto sub = cf->root_storage().open_storage("Sub");
        ASSERT_TRUE(sub.has_value());
        EXPECT_EQ(sub->children().size(), 2u);
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    storage_ptr w32sub;
    ASSERT_TRUE(
        SUCCEEDED(stg->OpenStorage(L"Sub", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0, w32sub.put())));
    EXPECT_EQ(win32_enumerate(w32sub.get()).size(), 2u);
}

// ── Enumerate deeply nested ─────────────────────────────────────────────

TEST_P(StressEnumerationConformance, DeepNestedEnumerate) {
    auto p = temp_file("se_deep");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto a = cf->root_storage().create_storage("A");
        ASSERT_TRUE(a.has_value());
        auto b = a->create_storage("B");
        ASSERT_TRUE(b.has_value());
        ASSERT_TRUE(b->create_stream("Leaf").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto a = cf->root_storage().open_storage("A");
    ASSERT_TRUE(a.has_value());
    auto b = a->open_storage("B");
    ASSERT_TRUE(b.has_value());
    auto kids = b->children();
    EXPECT_EQ(kids.size(), 1u);
    EXPECT_EQ(kids[0].name, "Leaf");
}

// ── Enumerate after rename ──────────────────────────────────────────────

TEST_P(StressEnumerationConformance, EnumerateAfterRename) {
    auto p = temp_file("se_ren");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        auto sub = root.create_storage("Old");
        ASSERT_TRUE(sub.has_value());
        ASSERT_TRUE(sub->rename("New").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto kids = cf->root_storage().children();
    EXPECT_EQ(kids.size(), 1u);
    EXPECT_EQ(kids[0].name, "New");
}

// ── Enumerate after resize ──────────────────────────────────────────────

TEST_P(StressEnumerationConformance, EnumerateAfterResize) {
    auto p = temp_file("se_rsz");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        auto s = root.create_stream("D");
        ASSERT_TRUE(s.has_value());
        auto data = make_test_data(100, 0x55);
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        ASSERT_TRUE(s->resize(5000).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto kids = cf->root_storage().children();
    EXPECT_EQ(kids.size(), 1u);
    EXPECT_EQ(kids[0].size, 5000u);
}

#endif // _WIN32
