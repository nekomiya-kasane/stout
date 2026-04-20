#ifdef _WIN32

#include "conformance_utils.h"

#include <gtest/gtest.h>
#include <set>
#include <stout/compound_file.h>

using namespace conformance;
using namespace stout;

struct VPRen {
    cfb_version ver;
    uint16_t major;
};

class StressRenameConformance : public ::testing::TestWithParam<VPRen> {
  protected:
    com_init com_;
    temp_file_guard guard_;
};

static const VPRen vp_ren[] = {{cfb_version::v3, 3}, {cfb_version::v4, 4}};

INSTANTIATE_TEST_SUITE_P(V, StressRenameConformance, ::testing::ValuesIn(vp_ren),
                         [](const auto &info) { return info.param.major == 3 ? "V3" : "V4"; });

// ── Storage rename ──────────────────────────────────────────────────────

TEST_P(StressRenameConformance, RenameStorageSimple) {
    auto p = temp_file("sr_simple");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto sub = cf->root_storage().create_storage("Old");
        ASSERT_TRUE(sub.has_value());
        ASSERT_TRUE(sub->rename("New").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    storage_ptr sub;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStorage(L"New", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0, sub.put())));
}

TEST_P(StressRenameConformance, RenameStorageWithChildren) {
    auto p = temp_file("sr_wchild");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto sub = cf->root_storage().create_storage("Parent");
        ASSERT_TRUE(sub.has_value());
        ASSERT_TRUE(sub->create_stream("Child").has_value());
        ASSERT_TRUE(sub->rename("Renamed").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    storage_ptr sub;
    ASSERT_TRUE(
        SUCCEEDED(stg->OpenStorage(L"Renamed", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0, sub.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(sub->OpenStream(L"Child", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
}

TEST_P(StressRenameConformance, RenameDeepNested) {
    auto p = temp_file("sr_deep");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto a = cf->root_storage().create_storage("A");
        ASSERT_TRUE(a.has_value());
        auto b = a->create_storage("B");
        ASSERT_TRUE(b.has_value());
        ASSERT_TRUE(b->rename("B2").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto a = cf->root_storage().open_storage("A");
    ASSERT_TRUE(a.has_value());
    auto b2 = a->open_storage("B2");
    ASSERT_TRUE(b2.has_value());
}

// ── Stream rename (NEW API) ─────────────────────────────────────────────

TEST_P(StressRenameConformance, RenameStreamSimple) {
    auto p = temp_file("sr_strm");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("OldStream");
        ASSERT_TRUE(s.has_value());
        auto data = make_test_data(200, 0xAA);
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        ASSERT_TRUE(s->rename("NewStream").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"NewStream", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 200u);
    std::vector<uint8_t> buf(200);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 200, &rc)));
    EXPECT_EQ(buf, make_test_data(200, 0xAA));
}

TEST_P(StressRenameConformance, RenameStreamPreservesData) {
    auto p = temp_file("sr_sdata");
    guard_.add(p);
    auto data = make_test_data(500, 0xBB);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("Before");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        ASSERT_TRUE(s->rename("After").has_value());
        // Read back in same session
        std::vector<uint8_t> buf(500);
        ASSERT_TRUE(s->read(0, std::span<uint8_t>(buf)).has_value());
        EXPECT_EQ(buf, data);
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto s = cf->root_storage().open_stream("After");
    ASSERT_TRUE(s.has_value());
    std::vector<uint8_t> buf(500);
    ASSERT_TRUE(s->read(0, std::span<uint8_t>(buf)).has_value());
    EXPECT_EQ(buf, data);
}

// ── Rename edge cases ───────────────────────────────────────────────────

TEST_P(StressRenameConformance, RenameEmptyNameFails) {
    auto p = temp_file("sr_empty");
    guard_.add(p);
    auto cf = compound_file::create(p, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    auto sub = cf->root_storage().create_storage("X");
    ASSERT_TRUE(sub.has_value());
    auto r = sub->rename("");
    EXPECT_FALSE(r.has_value());
}

TEST_P(StressRenameConformance, RenameTooLongFails) {
    auto p = temp_file("sr_long");
    guard_.add(p);
    auto cf = compound_file::create(p, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    auto sub = cf->root_storage().create_storage("X");
    ASSERT_TRUE(sub.has_value());
    std::string long_name(32, 'A'); // 32 chars > 31 max
    auto r = sub->rename(long_name);
    EXPECT_FALSE(r.has_value());
}

TEST_P(StressRenameConformance, RenameMaxLength31) {
    auto p = temp_file("sr_max31");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto sub = cf->root_storage().create_storage("X");
        ASSERT_TRUE(sub.has_value());
        std::string name31(31, 'Z');
        ASSERT_TRUE(sub->rename(name31).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto kids = cf->root_storage().children();
    EXPECT_EQ(kids.size(), 1u);
    EXPECT_EQ(kids[0].name, std::string(31, 'Z'));
}

// ── Win32 renames, Stout reads ──────────────────────────────────────────

TEST_P(StressRenameConformance, Win32RenameStoutReads) {
    auto p = temp_file("sr_w32ren");
    guard_.add(p);
    {
        storage_ptr stg;
        if (GetParam().ver == cfb_version::v4)
            ASSERT_TRUE(SUCCEEDED(win32_create_v4(p.wstring(), stg.put())));
        else
            ASSERT_TRUE(SUCCEEDED(win32_create_v3(p.wstring(), stg.put())));
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(
            stg->CreateStream(L"Original", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, strm.put())));
        strm.reset();
        ASSERT_TRUE(SUCCEEDED(stg->RenameElement(L"Original", L"Renamed")));
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto kids = cf->root_storage().children();
    EXPECT_EQ(kids.size(), 1u);
    EXPECT_EQ(kids[0].name, "Renamed");
}

// ── Rename then enumerate ───────────────────────────────────────────────

TEST_P(StressRenameConformance, RenameOldGoneNewPresent) {
    auto p = temp_file("sr_enum");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        auto sub = root.create_storage("Alpha");
        ASSERT_TRUE(sub.has_value());
        ASSERT_TRUE(sub->rename("Beta").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    EXPECT_FALSE(cf->root_storage().exists("Alpha"));
    EXPECT_TRUE(cf->root_storage().exists("Beta"));
}

// ── Rename preserves CLSID/state bits ───────────────────────────────────

TEST_P(StressRenameConformance, RenamePreservesClsid) {
    auto p = temp_file("sr_clsid");
    guard_.add(p);
    stout::guid test_id{0x11223344, 0x5566, 0x7788, {0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00}};
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto sub = cf->root_storage().create_storage("X");
        ASSERT_TRUE(sub.has_value());
        sub->set_clsid(test_id);
        ASSERT_TRUE(sub->rename("Y").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto sub = cf->root_storage().open_storage("Y");
    ASSERT_TRUE(sub.has_value());
    EXPECT_EQ(sub->clsid(), test_id);
}

TEST_P(StressRenameConformance, RenamePreservesStateBits) {
    auto p = temp_file("sr_bits");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto sub = cf->root_storage().create_storage("X");
        ASSERT_TRUE(sub.has_value());
        sub->set_state_bits(0xDEADBEEF);
        ASSERT_TRUE(sub->rename("Y").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto sub = cf->root_storage().open_storage("Y");
    ASSERT_TRUE(sub.has_value());
    EXPECT_EQ(sub->state_bits(), 0xDEADBEEFu);
}

// ── Multiple renames ────────────────────────────────────────────────────

TEST_P(StressRenameConformance, MultipleRenames) {
    auto p = temp_file("sr_multi");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto sub = cf->root_storage().create_storage("A");
        ASSERT_TRUE(sub.has_value());
        ASSERT_TRUE(sub->rename("B").has_value());
        ASSERT_TRUE(sub->rename("C").has_value());
        ASSERT_TRUE(sub->rename("D").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto kids = cf->root_storage().children();
    EXPECT_EQ(kids.size(), 1u);
    EXPECT_EQ(kids[0].name, "D");
}

// ── Rename after delete ─────────────────────────────────────────────────

TEST_P(StressRenameConformance, RenameAfterDeleteOther) {
    auto p = temp_file("sr_dren");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        ASSERT_TRUE(root.create_storage("A").has_value());
        auto b = root.create_storage("B");
        ASSERT_TRUE(b.has_value());
        ASSERT_TRUE(root.remove("A").has_value());
        ASSERT_TRUE(b->rename("C").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto kids = cf->root_storage().children();
    EXPECT_EQ(kids.size(), 1u);
    EXPECT_EQ(kids[0].name, "C");
}

// ── Stream rename empty name fails ──────────────────────────────────────

TEST_P(StressRenameConformance, StreamRenameEmptyFails) {
    auto p = temp_file("sr_sempty");
    guard_.add(p);
    auto cf = compound_file::create(p, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    auto s = cf->root_storage().create_stream("X");
    ASSERT_TRUE(s.has_value());
    EXPECT_FALSE(s->rename("").has_value());
}

TEST_P(StressRenameConformance, StreamRenameTooLongFails) {
    auto p = temp_file("sr_slong");
    guard_.add(p);
    auto cf = compound_file::create(p, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    auto s = cf->root_storage().create_stream("X");
    ASSERT_TRUE(s.has_value());
    EXPECT_FALSE(s->rename(std::string(32, 'A')).has_value());
}

#endif // _WIN32
