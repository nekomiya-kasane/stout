#ifdef _WIN32

#    include "conformance_utils.h"

#    include <gtest/gtest.h>
#    include <stout/compound_file.h>

using namespace conformance;
using namespace stout;

struct VPHier {
    cfb_version ver;
    uint16_t major;
};

class StressHierarchyConformance : public ::testing::TestWithParam<VPHier> {
  protected:
    com_init com_;
    temp_file_guard guard_;
};

static const VPHier vp_hier[] = {{cfb_version::v3, 3}, {cfb_version::v4, 4}};

INSTANTIATE_TEST_SUITE_P(V, StressHierarchyConformance, ::testing::ValuesIn(vp_hier),
                         [](const auto &info) { return info.param.major == 3 ? "V3" : "V4"; });

// ── Single level with many children ─────────────────────────────────────

TEST_P(StressHierarchyConformance, SingleLevel20Children) {
    auto p = temp_file("hi_20ch");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        for (int i = 0; i < 20; ++i) {
            auto s = cf->root_storage().create_stream("C" + std::to_string(i));
            ASSERT_TRUE(s.has_value());
            auto d = make_test_data(50, static_cast<uint8_t>(i));
            ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        }
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    EXPECT_EQ(win32_enumerate(stg.get()).size(), 20u);
}

// ── Two levels: 5 storages × 4 streams ──────────────────────────────────

TEST_P(StressHierarchyConformance, TwoLevels5x4) {
    auto p = temp_file("hi_5x4");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        for (int i = 0; i < 5; ++i) {
            auto sub = cf->root_storage().create_storage("D" + std::to_string(i));
            ASSERT_TRUE(sub.has_value());
            for (int j = 0; j < 4; ++j) {
                auto s = sub->create_stream("S" + std::to_string(j));
                ASSERT_TRUE(s.has_value());
                auto d = make_test_data(100, static_cast<uint8_t>(i * 4 + j));
                ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
            }
        }
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    auto root_entries = win32_enumerate(stg.get());
    EXPECT_EQ(root_entries.size(), 5u);
    for (auto &e : root_entries) {
        free_statstg_name(e);
    }
    for (int i = 0; i < 5; ++i) {
        auto name = L"D" + std::to_wstring(i);
        storage_ptr sub;
        ASSERT_TRUE(SUCCEEDED(
            stg->OpenStorage(name.c_str(), nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0, sub.put())));
        auto sub_entries = win32_enumerate(sub.get());
        EXPECT_EQ(sub_entries.size(), 4u);
        for (auto &e : sub_entries) {
            free_statstg_name(e);
        }
    }
}

// ── Three levels deep ───────────────────────────────────────────────────

TEST_P(StressHierarchyConformance, ThreeLevelsDeep) {
    auto p = temp_file("hi_3deep");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto l1 = cf->root_storage().create_storage("L1");
        ASSERT_TRUE(l1.has_value());
        auto l2 = l1->create_storage("L2");
        ASSERT_TRUE(l2.has_value());
        auto l3 = l2->create_storage("L3");
        ASSERT_TRUE(l3.has_value());
        auto s = l3->create_stream("Leaf");
        ASSERT_TRUE(s.has_value());
        auto d = make_test_data(200, 0xAA);
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    storage_ptr l1, l2, l3;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStorage(L"L1", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0, l1.put())));
    ASSERT_TRUE(SUCCEEDED(l1->OpenStorage(L"L2", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0, l2.put())));
    ASSERT_TRUE(SUCCEEDED(l2->OpenStorage(L"L3", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0, l3.put())));
    stream_ptr leaf;
    ASSERT_TRUE(SUCCEEDED(l3->OpenStream(L"Leaf", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, leaf.put())));
    EXPECT_EQ(win32_stream_size(leaf.get()), 200u);
}

// ── Mixed streams and storages at each level ────────────────────────────

TEST_P(StressHierarchyConformance, MixedAtEachLevel) {
    auto p = temp_file("hi_mixed");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->root_storage().create_stream("RootStream").has_value());
        auto sub = cf->root_storage().create_storage("Sub");
        ASSERT_TRUE(sub.has_value());
        auto s = sub->create_stream("SubStream");
        ASSERT_TRUE(s.has_value());
        auto d = make_test_data(150, 0xBB);
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        auto subsub = sub->create_storage("SubSub");
        ASSERT_TRUE(subsub.has_value());
        auto ss = subsub->create_stream("DeepStream");
        ASSERT_TRUE(ss.has_value());
        auto d2 = make_test_data(250, 0xCC);
        ASSERT_TRUE(ss->write(0, std::span<const uint8_t>(d2)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    EXPECT_EQ(cf->root_storage().children().size(), 2u);
    auto sub = cf->root_storage().open_storage("Sub");
    ASSERT_TRUE(sub.has_value());
    EXPECT_EQ(sub->children().size(), 2u);
}

// ── Empty storages at multiple levels ───────────────────────────────────

TEST_P(StressHierarchyConformance, EmptyStoragesMultipleLevels) {
    auto p = temp_file("hi_empty");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto a = cf->root_storage().create_storage("A");
        ASSERT_TRUE(a.has_value());
        auto b = a->create_storage("B");
        ASSERT_TRUE(b.has_value());
        auto c = b->create_storage("C");
        ASSERT_TRUE(c.has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    storage_ptr a, b, c;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStorage(L"A", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0, a.put())));
    ASSERT_TRUE(SUCCEEDED(a->OpenStorage(L"B", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0, b.put())));
    ASSERT_TRUE(SUCCEEDED(b->OpenStorage(L"C", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0, c.put())));
    EXPECT_EQ(win32_enumerate(c.get()).size(), 0u);
}

// ── Sibling storages with streams ───────────────────────────────────────

TEST_P(StressHierarchyConformance, SiblingStoragesWithStreams) {
    auto p = temp_file("hi_sibling");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        for (int i = 0; i < 3; ++i) {
            auto sub = cf->root_storage().create_storage("Dir" + std::to_string(i));
            ASSERT_TRUE(sub.has_value());
            for (int j = 0; j < 3; ++j) {
                auto s = sub->create_stream("F" + std::to_string(j));
                ASSERT_TRUE(s.has_value());
                auto d = make_test_data(100 + j * 50, static_cast<uint8_t>(i * 3 + j));
                ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
            }
        }
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    for (int i = 0; i < 3; ++i) {
        auto dname = L"Dir" + std::to_wstring(i);
        storage_ptr sub;
        ASSERT_TRUE(SUCCEEDED(
            stg->OpenStorage(dname.c_str(), nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0, sub.put())));
        for (int j = 0; j < 3; ++j) {
            auto sname = L"F" + std::to_wstring(j);
            stream_ptr strm;
            ASSERT_TRUE(
                SUCCEEDED(sub->OpenStream(sname.c_str(), nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
            EXPECT_EQ(win32_stream_size(strm.get()), static_cast<uint64_t>(100 + j * 50));
        }
    }
}

// ── Win32 creates hierarchy, Stout reads ────────────────────────────────

TEST_P(StressHierarchyConformance, Win32HierarchyStoutReads) {
    auto p = temp_file("hi_w32");
    guard_.add(p);
    {
        storage_ptr stg;
        if (GetParam().ver == cfb_version::v4) {
            ASSERT_TRUE(SUCCEEDED(win32_create_v4(p.wstring(), stg.put())));
        } else {
            ASSERT_TRUE(SUCCEEDED(win32_create_v3(p.wstring(), stg.put())));
        }
        storage_ptr sub;
        ASSERT_TRUE(SUCCEEDED(
            stg->CreateStorage(L"Sub", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, sub.put())));
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(
            sub->CreateStream(L"Data", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, strm.put())));
        auto d = make_test_data(300, 0xDD);
        ASSERT_TRUE(SUCCEEDED(win32_stream_write(strm.get(), d.data(), 300)));
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto sub = cf->root_storage().open_storage("Sub");
    ASSERT_TRUE(sub.has_value());
    auto s = sub->open_stream("Data");
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->size(), 300u);
    std::vector<uint8_t> buf(300);
    ASSERT_TRUE(s->read(0, std::span<uint8_t>(buf)).has_value());
    EXPECT_EQ(buf, make_test_data(300, 0xDD));
}

// ── Storage with stream and sub-storage same name prefix ────────────────

TEST_P(StressHierarchyConformance, SameNamePrefixDifferentTypes) {
    auto p = temp_file("hi_prefix");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->root_storage().create_stream("Item").has_value());
        ASSERT_TRUE(cf->root_storage().create_storage("ItemDir").has_value());
        ASSERT_TRUE(cf->root_storage().create_stream("ItemX").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    auto entries = win32_enumerate(stg.get());
    EXPECT_EQ(entries.size(), 3u);
    for (auto &e : entries) {
        free_statstg_name(e);
    }
}

// ── Delete sub-storage, verify parent ───────────────────────────────────

TEST_P(StressHierarchyConformance, DeleteSubStorageVerifyParent) {
    auto p = temp_file("hi_delsub");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto sub = cf->root_storage().create_storage("ToDelete");
        ASSERT_TRUE(sub.has_value());
        ASSERT_TRUE(sub->create_stream("Inner").has_value());
        ASSERT_TRUE(cf->root_storage().create_stream("Keep").has_value());
        ASSERT_TRUE(cf->root_storage().remove("ToDelete").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto kids = cf->root_storage().children();
    EXPECT_EQ(kids.size(), 1u);
    EXPECT_EQ(kids[0].name, "Keep");
}

#endif // _WIN32
