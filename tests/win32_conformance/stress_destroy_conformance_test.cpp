#ifdef _WIN32

#include "conformance_utils.h"
#include <stout/compound_file.h>
#include <gtest/gtest.h>

using namespace conformance;
using namespace stout;

struct VPDest {
    cfb_version ver;
    uint16_t major;
};

class StressDestroyConformance : public ::testing::TestWithParam<VPDest> {
protected:
    com_init com_;
    temp_file_guard guard_;
};

static const VPDest vp_dest[] = {{cfb_version::v3, 3}, {cfb_version::v4, 4}};

INSTANTIATE_TEST_SUITE_P(V, StressDestroyConformance, ::testing::ValuesIn(vp_dest),
    [](const auto& info) { return info.param.major == 3 ? "V3" : "V4"; });

// ── Destroy stream ──────────────────────────────────────────────────────

TEST_P(StressDestroyConformance, DestroyStream) {
    auto p = temp_file("sd_strm"); guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("X");
        ASSERT_TRUE(s.has_value());
        auto data = make_test_data(200, 0x11);
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        ASSERT_TRUE(cf->root_storage().remove("X").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    EXPECT_EQ(cf->root_storage().children().size(), 0u);
}

TEST_P(StressDestroyConformance, DestroyStorage) {
    auto p = temp_file("sd_stg"); guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->root_storage().create_storage("Dir").has_value());
        ASSERT_TRUE(cf->root_storage().remove("Dir").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    EXPECT_EQ(cf->root_storage().children().size(), 0u);
}

TEST_P(StressDestroyConformance, DestroyNonExistent) {
    auto p = temp_file("sd_noex"); guard_.add(p);
    auto cf = compound_file::create(p, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    auto r = cf->root_storage().remove("Ghost");
    EXPECT_FALSE(r.has_value());
}

// ── Destroy with children ───────────────────────────────────────────────

TEST_P(StressDestroyConformance, DestroyStorageWithChildren) {
    auto p = temp_file("sd_wchild"); guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto sub = cf->root_storage().create_storage("Parent");
        ASSERT_TRUE(sub.has_value());
        ASSERT_TRUE(sub->create_stream("Child1").has_value());
        ASSERT_TRUE(sub->create_stream("Child2").has_value());
        ASSERT_TRUE(cf->root_storage().remove("Parent").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    EXPECT_EQ(cf->root_storage().children().size(), 0u);
}

// ── Destroy first, middle, last sibling ─────────────────────────────────

TEST_P(StressDestroyConformance, DestroyFirstSibling) {
    auto p = temp_file("sd_first"); guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        ASSERT_TRUE(root.create_stream("A").has_value());
        ASSERT_TRUE(root.create_stream("B").has_value());
        ASSERT_TRUE(root.create_stream("C").has_value());
        ASSERT_TRUE(root.remove("A").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    EXPECT_EQ(cf->root_storage().children().size(), 2u);
    EXPECT_FALSE(cf->root_storage().exists("A"));
}

TEST_P(StressDestroyConformance, DestroyMiddleSibling) {
    auto p = temp_file("sd_mid"); guard_.add(p);
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
    EXPECT_EQ(cf->root_storage().children().size(), 2u);
    EXPECT_TRUE(cf->root_storage().exists("A"));
    EXPECT_TRUE(cf->root_storage().exists("C"));
}

TEST_P(StressDestroyConformance, DestroyLastSibling) {
    auto p = temp_file("sd_last"); guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        ASSERT_TRUE(root.create_stream("A").has_value());
        ASSERT_TRUE(root.create_stream("B").has_value());
        ASSERT_TRUE(root.create_stream("C").has_value());
        ASSERT_TRUE(root.remove("C").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    EXPECT_EQ(cf->root_storage().children().size(), 2u);
    EXPECT_FALSE(cf->root_storage().exists("C"));
}

// ── Destroy all one by one ──────────────────────────────────────────────

TEST_P(StressDestroyConformance, DestroyAllOneByOne) {
    auto p = temp_file("sd_all"); guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        for (int i = 0; i < 5; ++i)
            ASSERT_TRUE(root.create_stream("S" + std::to_string(i)).has_value());
        for (int i = 0; i < 5; ++i)
            ASSERT_TRUE(root.remove("S" + std::to_string(i)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    EXPECT_EQ(cf->root_storage().children().size(), 0u);
}

// ── Destroy and recreate same name ──────────────────────────────────────

TEST_P(StressDestroyConformance, DestroyAndRecreateSameName) {
    auto p = temp_file("sd_recreate"); guard_.add(p);
    auto data = make_test_data(300, 0x77);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        auto s = root.create_stream("X");
        ASSERT_TRUE(s.has_value());
        auto old_data = make_test_data(100, 0x11);
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(old_data)).has_value());
        ASSERT_TRUE(root.remove("X").has_value());
        auto s2 = root.create_stream("X");
        ASSERT_TRUE(s2.has_value());
        ASSERT_TRUE(s2->write(0, std::span<const uint8_t>(data)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto s = cf->root_storage().open_stream("X");
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->size(), 300u);
    std::vector<uint8_t> buf(300);
    ASSERT_TRUE(s->read(0, std::span<uint8_t>(buf)).has_value());
    EXPECT_EQ(buf, data);
}

// ── Win32 destroys, Stout verifies ──────────────────────────────────────

TEST_P(StressDestroyConformance, Win32DestroyStoutVerifies) {
    auto p = temp_file("sd_w32"); guard_.add(p);
    {
        storage_ptr stg;
        if (GetParam().ver == cfb_version::v4) ASSERT_TRUE(SUCCEEDED(win32_create_v4(p.wstring(), stg.put())));
        else ASSERT_TRUE(SUCCEEDED(win32_create_v3(p.wstring(), stg.put())));
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(stg->CreateStream(L"Victim",
            STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, strm.put())));
        strm.reset();
        ASSERT_TRUE(SUCCEEDED(stg->DestroyElement(L"Victim")));
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    EXPECT_EQ(cf->root_storage().children().size(), 0u);
}

// ── Stout destroys, Win32 verifies ──────────────────────────────────────

TEST_P(StressDestroyConformance, StoutDestroyWin32Verifies) {
    auto p = temp_file("sd_s2w"); guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        ASSERT_TRUE(root.create_stream("Keep").has_value());
        ASSERT_TRUE(root.create_stream("Remove").has_value());
        ASSERT_TRUE(root.remove("Remove").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    auto entries = win32_enumerate(stg.get());
    EXPECT_EQ(entries.size(), 1u);
    EXPECT_EQ(std::wstring(entries[0].pwcsName), L"Keep");
    free_statstg_name(entries[0]);
}

// ── Destroy in sub-storage ──────────────────────────────────────────────

TEST_P(StressDestroyConformance, DestroyInSubStorage) {
    auto p = temp_file("sd_sub"); guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto sub = cf->root_storage().create_storage("Sub");
        ASSERT_TRUE(sub.has_value());
        ASSERT_TRUE(sub->create_stream("A").has_value());
        ASSERT_TRUE(sub->create_stream("B").has_value());
        ASSERT_TRUE(sub->remove("A").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto sub = cf->root_storage().open_storage("Sub");
    ASSERT_TRUE(sub.has_value());
    EXPECT_EQ(sub->children().size(), 1u);
    EXPECT_EQ(sub->children()[0].name, "B");
}

// ── Destroy mini stream, others unaffected ──────────────────────────────

TEST_P(StressDestroyConformance, DestroyMiniOthersUnaffected) {
    auto p = temp_file("sd_mini"); guard_.add(p);
    auto data_b = make_test_data(200, 0x22);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        auto sa = root.create_stream("A");
        ASSERT_TRUE(sa.has_value());
        auto da = make_test_data(100, 0x11);
        ASSERT_TRUE(sa->write(0, std::span<const uint8_t>(da)).has_value());
        auto sb = root.create_stream("B");
        ASSERT_TRUE(sb.has_value());
        ASSERT_TRUE(sb->write(0, std::span<const uint8_t>(data_b)).has_value());
        ASSERT_TRUE(root.remove("A").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto s = cf->root_storage().open_stream("B");
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->size(), 200u);
    std::vector<uint8_t> buf(200);
    ASSERT_TRUE(s->read(0, std::span<uint8_t>(buf)).has_value());
    EXPECT_EQ(buf, data_b);
}

// ── Destroy regular stream, others unaffected ───────────────────────────

TEST_P(StressDestroyConformance, DestroyRegularOthersUnaffected) {
    auto p = temp_file("sd_reg"); guard_.add(p);
    auto data_b = make_test_data(5000, 0x22);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        auto sa = root.create_stream("A");
        ASSERT_TRUE(sa.has_value());
        auto da = make_test_data(6000, 0x11);
        ASSERT_TRUE(sa->write(0, std::span<const uint8_t>(da)).has_value());
        auto sb = root.create_stream("B");
        ASSERT_TRUE(sb.has_value());
        ASSERT_TRUE(sb->write(0, std::span<const uint8_t>(data_b)).has_value());
        ASSERT_TRUE(root.remove("A").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto s = cf->root_storage().open_stream("B");
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->size(), 5000u);
    std::vector<uint8_t> buf(5000);
    ASSERT_TRUE(s->read(0, std::span<uint8_t>(buf)).has_value());
    EXPECT_EQ(buf, data_b);
}

#endif // _WIN32
