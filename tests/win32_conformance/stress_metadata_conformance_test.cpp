#ifdef _WIN32

#include "conformance_utils.h"

#include <chrono>
#include <gtest/gtest.h>
#include <stout/compound_file.h>

using namespace conformance;
using namespace stout;

struct VPMeta {
    cfb_version ver;
    uint16_t major;
};

class StressMetadataConformance : public ::testing::TestWithParam<VPMeta> {
  protected:
    com_init com_;
    temp_file_guard guard_;
};

static const VPMeta vp_meta[] = {{cfb_version::v3, 3}, {cfb_version::v4, 4}};

INSTANTIATE_TEST_SUITE_P(V, StressMetadataConformance, ::testing::ValuesIn(vp_meta),
                         [](const auto &info) { return info.param.major == 3 ? "V3" : "V4"; });

// ── CLSID ───────────────────────────────────────────────────────────────

TEST_P(StressMetadataConformance, ClsidNullByDefault) {
    auto p = temp_file("sm_clsnull");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    EXPECT_TRUE(cf->root_storage().clsid().is_null());
}

TEST_P(StressMetadataConformance, ClsidSetGetRoot) {
    auto p = temp_file("sm_clsroot");
    guard_.add(p);
    stout::guid id{0xAABBCCDD, 0x1122, 0x3344, {1, 2, 3, 4, 5, 6, 7, 8}};
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        cf->root_storage().set_clsid(id);
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    EXPECT_EQ(cf->root_storage().clsid(), id);
}

TEST_P(StressMetadataConformance, ClsidSetGetSubStorage) {
    auto p = temp_file("sm_clssub");
    guard_.add(p);
    stout::guid id{0x11111111, 0x2222, 0x3333, {4, 5, 6, 7, 8, 9, 10, 11}};
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto sub = cf->root_storage().create_storage("Sub");
        ASSERT_TRUE(sub.has_value());
        sub->set_clsid(id);
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto sub = cf->root_storage().open_storage("Sub");
    ASSERT_TRUE(sub.has_value());
    EXPECT_EQ(sub->clsid(), id);
}

TEST_P(StressMetadataConformance, ClsidStoutWriteWin32Read) {
    auto p = temp_file("sm_cls_s2w");
    guard_.add(p);
    stout::guid id{0xDEADBEEF, 0xCAFE, 0xBABE, {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0}};
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        cf->root_storage().set_clsid(id);
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    STATSTG st{};
    ASSERT_TRUE(SUCCEEDED(stg->Stat(&st, STATFLAG_NONAME)));
    EXPECT_EQ(st.clsid.Data1, id.data1);
    EXPECT_EQ(st.clsid.Data2, id.data2);
    EXPECT_EQ(st.clsid.Data3, id.data3);
}

TEST_P(StressMetadataConformance, ClsidWin32WriteStoutRead) {
    auto p = temp_file("sm_cls_w2s");
    guard_.add(p);
    CLSID test_clsid;
    ASSERT_TRUE(SUCCEEDED(CoCreateGuid(&test_clsid)));
    {
        storage_ptr stg;
        if (GetParam().ver == cfb_version::v4) {
            ASSERT_TRUE(SUCCEEDED(win32_create_v4(p.wstring(), stg.put())));
        } else {
            ASSERT_TRUE(SUCCEEDED(win32_create_v3(p.wstring(), stg.put())));
        }
        ASSERT_TRUE(SUCCEEDED(stg->SetClass(test_clsid)));
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto c = cf->root_storage().clsid();
    EXPECT_EQ(c.data1, test_clsid.Data1);
    EXPECT_EQ(c.data2, test_clsid.Data2);
    EXPECT_EQ(c.data3, test_clsid.Data3);
}

// ── State bits ──────────────────────────────────────────────────────────

TEST_P(StressMetadataConformance, StateBitsZeroByDefault) {
    auto p = temp_file("sm_bits0");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    EXPECT_EQ(cf->root_storage().state_bits(), 0u);
}

TEST_P(StressMetadataConformance, StateBitsAllOnes) {
    auto p = temp_file("sm_bitsff");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        cf->root_storage().set_state_bits(0xFFFFFFFF);
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    EXPECT_EQ(cf->root_storage().state_bits(), 0xFFFFFFFFu);
}

TEST_P(StressMetadataConformance, StateBitsWithMask) {
    auto p = temp_file("sm_bitsmask");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        cf->root_storage().set_state_bits(0xFF00FF00);
        cf->root_storage().set_state_bits(0x0000CAFE, 0x0000FFFF);
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    EXPECT_EQ(cf->root_storage().state_bits(), 0xFF00CAFEu);
}

TEST_P(StressMetadataConformance, StateBitsStoutWriteWin32Read) {
    auto p = temp_file("sm_bits_s2w");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        cf->root_storage().set_state_bits(0xDEADBEEF);
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    STATSTG st{};
    ASSERT_TRUE(SUCCEEDED(stg->Stat(&st, STATFLAG_NONAME)));
    EXPECT_EQ(st.grfStateBits, 0xDEADBEEFu);
}

TEST_P(StressMetadataConformance, StateBitsWin32WriteStoutRead) {
    auto p = temp_file("sm_bits_w2s");
    guard_.add(p);
    {
        storage_ptr stg;
        if (GetParam().ver == cfb_version::v4) {
            ASSERT_TRUE(SUCCEEDED(win32_create_v4(p.wstring(), stg.put())));
        } else {
            ASSERT_TRUE(SUCCEEDED(win32_create_v3(p.wstring(), stg.put())));
        }
        ASSERT_TRUE(SUCCEEDED(stg->SetStateBits(0xCAFEBABE, 0xFFFFFFFF)));
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    EXPECT_EQ(cf->root_storage().state_bits(), 0xCAFEBABEu);
}

// ── Timestamps ──────────────────────────────────────────────────────────

TEST_P(StressMetadataConformance, TimestampSetGet) {
    auto p = temp_file("sm_ts");
    guard_.add(p);
    auto tp = std::chrono::sys_days{std::chrono::year{2020} / std::chrono::January / 1};
    auto ft = stout::file_time{tp};
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto sub = cf->root_storage().create_storage("T");
        ASSERT_TRUE(sub.has_value());
        sub->set_creation_time(ft);
        sub->set_modified_time(ft);
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto sub = cf->root_storage().open_storage("T");
    ASSERT_TRUE(sub.has_value());
    // Timestamps should be non-zero
    auto ct = sub->creation_time();
    auto mt = sub->modified_time();
    EXPECT_EQ(ct, ft);
    EXPECT_EQ(mt, ft);
}

TEST_P(StressMetadataConformance, TimestampStoutWriteWin32Read) {
    auto p = temp_file("sm_ts_s2w");
    guard_.add(p);
    auto tp = std::chrono::sys_days{std::chrono::year{2023} / std::chrono::June / 15};
    auto ft = stout::file_time{tp};
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto sub = cf->root_storage().create_storage("T");
        ASSERT_TRUE(sub.has_value());
        sub->set_creation_time(ft);
        sub->set_modified_time(ft);
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    storage_ptr sub;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStorage(L"T", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0, sub.put())));
    STATSTG st{};
    ASSERT_TRUE(SUCCEEDED(sub->Stat(&st, STATFLAG_NONAME)));
    ULARGE_INTEGER ctime, mtime;
    ctime.LowPart = st.ctime.dwLowDateTime;
    ctime.HighPart = st.ctime.dwHighDateTime;
    mtime.LowPart = st.mtime.dwLowDateTime;
    mtime.HighPart = st.mtime.dwHighDateTime;
    EXPECT_GT(ctime.QuadPart, 0u);
    EXPECT_EQ(ctime.QuadPart, mtime.QuadPart);
}

// ── set_element_times (NEW API) ─────────────────────────────────────────

TEST_P(StressMetadataConformance, SetElementTimesOnChild) {
    auto p = temp_file("sm_elt");
    guard_.add(p);
    auto tp = std::chrono::sys_days{std::chrono::year{2025} / std::chrono::March / 1};
    auto ft = stout::file_time{tp};
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        ASSERT_TRUE(root.create_storage("Child").has_value());
        auto r = root.set_element_times("Child", ft, ft);
        ASSERT_TRUE(r.has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto child = cf->root_storage().open_storage("Child");
    ASSERT_TRUE(child.has_value());
    EXPECT_EQ(child->creation_time(), ft);
    EXPECT_EQ(child->modified_time(), ft);
}

TEST_P(StressMetadataConformance, SetElementTimesNotFound) {
    auto p = temp_file("sm_elt_nf");
    guard_.add(p);
    auto tp = std::chrono::sys_days{std::chrono::year{2020} / std::chrono::January / 1};
    auto ft = stout::file_time{tp};
    auto cf = compound_file::create(p, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    auto r = cf->root_storage().set_element_times("Ghost", ft, ft);
    EXPECT_FALSE(r.has_value());
}

// ── Entry stat cross-validation ─────────────────────────────────────────

TEST_P(StressMetadataConformance, EntryStatNameMatch) {
    auto p = temp_file("sm_statname");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("MyFile");
        ASSERT_TRUE(s.has_value());
        auto data = make_test_data(123, 0x55);
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    {
        auto cf = compound_file::open(p, open_mode::read);
        ASSERT_TRUE(cf.has_value());
        auto kids = cf->root_storage().children();
        ASSERT_EQ(kids.size(), 1u);
        EXPECT_EQ(kids[0].name, "MyFile");
        EXPECT_EQ(kids[0].type, entry_type::stream);
        EXPECT_EQ(kids[0].size, 123u);
    }
    // Win32 cross-check
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    auto w32 = win32_enumerate(stg.get());
    ASSERT_EQ(w32.size(), 1u);
    EXPECT_EQ(std::wstring(w32[0].pwcsName), L"MyFile");
    EXPECT_EQ(w32[0].type, STGTY_STREAM);
    EXPECT_EQ(w32[0].cbSize.QuadPart, 123u);
    free_statstg_name(w32[0]);
}

TEST_P(StressMetadataConformance, StorageStatTypeMatch) {
    auto p = temp_file("sm_stgtype");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->root_storage().create_storage("Dir").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    {
        auto cf = compound_file::open(p, open_mode::read);
        ASSERT_TRUE(cf.has_value());
        auto kids = cf->root_storage().children();
        ASSERT_EQ(kids.size(), 1u);
        EXPECT_EQ(kids[0].type, entry_type::storage);
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    auto w32 = win32_enumerate(stg.get());
    ASSERT_EQ(w32.size(), 1u);
    EXPECT_EQ(w32[0].type, STGTY_STORAGE);
    free_statstg_name(w32[0]);
}

// ── Metadata survives flush/reopen ──────────────────────────────────────

TEST_P(StressMetadataConformance, MetadataSurvivesReopen) {
    auto p = temp_file("sm_survive");
    guard_.add(p);
    stout::guid id{0xFEEDFACE, 0x1234, 0x5678, {9, 10, 11, 12, 13, 14, 15, 16}};
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        cf->root_storage().set_clsid(id);
        cf->root_storage().set_state_bits(0x42424242);
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    EXPECT_EQ(cf->root_storage().clsid(), id);
    EXPECT_EQ(cf->root_storage().state_bits(), 0x42424242u);
}

#endif // _WIN32
