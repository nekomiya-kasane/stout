#ifdef _WIN32

#include "conformance_utils.h"

#include <gtest/gtest.h>
#include <stout/compound_file.h>

using namespace conformance;
using namespace stout;

class MetadataConformance : public ::testing::Test {
  protected:
    com_init com_;
    temp_file_guard guard_;
};

// ── StatStreamSize: write N bytes, verify Stat().cbSize matches ─────────
TEST_F(MetadataConformance, StatStreamSize) {
    auto path = temp_file("meta_sz");
    guard_.add(path);

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("Data");
        ASSERT_TRUE(s.has_value());
        auto data = make_test_data(1234, 0x55);
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }

    // Win32 checks size
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"Data", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 1234u);
}

// ── StatStreamType: Win32 reports STGTY_STREAM for streams ──────────────
TEST_F(MetadataConformance, StatStreamType) {
    auto path = temp_file("meta_stype");
    guard_.add(path);

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("MyStream");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }

    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    auto entries = win32_enumerate(stg.get());
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].type, STGTY_STREAM);
    free_statstg_name(entries[0]);

    // Stout also reports stream
    auto cf = compound_file::open(path, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto children = cf->root_storage().children();
    ASSERT_EQ(children.size(), 1u);
    EXPECT_EQ(children[0].type, entry_type::stream);
}

// ── StatStorageType: Win32 reports STGTY_STORAGE for storages ───────────
TEST_F(MetadataConformance, StatStorageType) {
    auto path = temp_file("meta_stgtype");
    guard_.add(path);

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->root_storage().create_storage("MyDir").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }

    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    auto entries = win32_enumerate(stg.get());
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].type, STGTY_STORAGE);
    free_statstg_name(entries[0]);

    auto cf = compound_file::open(path, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto children = cf->root_storage().children();
    ASSERT_EQ(children.size(), 1u);
    EXPECT_EQ(children[0].type, entry_type::storage);
}

// ── Win32 sets CLSID, Stout reads it ────────────────────────────────────
TEST_F(MetadataConformance, Win32ClsidStoutRead) {
    auto path = temp_file("meta_clsid");
    guard_.add(path);

    CLSID test_clsid;
    ASSERT_TRUE(SUCCEEDED(CoCreateGuid(&test_clsid)));

    {
        storage_ptr stg;
        ASSERT_TRUE(SUCCEEDED(win32_create_v4(path.wstring(), stg.put())));
        ASSERT_TRUE(SUCCEEDED(stg->SetClass(test_clsid)));
    }

    auto cf = compound_file::open(path, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto st = cf->root_storage().stat();

    // Compare CLSID bytes
    EXPECT_EQ(st.clsid.data1, test_clsid.Data1);
    EXPECT_EQ(st.clsid.data2, test_clsid.Data2);
    EXPECT_EQ(st.clsid.data3, test_clsid.Data3);
    EXPECT_EQ(0, std::memcmp(st.clsid.data4.data(), test_clsid.Data4, 8));
}

// ── Win32 sets state bits, Stout reads them ─────────────────────────────
TEST_F(MetadataConformance, Win32StateBitsStoutRead) {
    auto path = temp_file("meta_bits");
    guard_.add(path);

    {
        storage_ptr stg;
        ASSERT_TRUE(SUCCEEDED(win32_create_v4(path.wstring(), stg.put())));
        ASSERT_TRUE(SUCCEEDED(stg->SetStateBits(0xDEADBEEF, 0xFFFFFFFF)));
    }

    auto cf = compound_file::open(path, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto st = cf->root_storage().stat();
    EXPECT_EQ(st.state_bits, 0xDEADBEEFu);
}

// ── Win32 sets state bits with mask, Stout reads ────────────────────────
TEST_F(MetadataConformance, Win32StateBitsMaskStoutRead) {
    auto path = temp_file("meta_mask");
    guard_.add(path);

    {
        storage_ptr stg;
        ASSERT_TRUE(SUCCEEDED(win32_create_v4(path.wstring(), stg.put())));
        // Set lower 16 bits only
        ASSERT_TRUE(SUCCEEDED(stg->SetStateBits(0xCAFE, 0x0000FFFF)));
    }

    auto cf = compound_file::open(path, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto st = cf->root_storage().stat();
    EXPECT_EQ(st.state_bits & 0xFFFF, 0xCAFEu);
}

// ── Stout stream stat matches Win32 stat ────────────────────────────────
TEST_F(MetadataConformance, StreamStatCrossMatch) {
    auto path = temp_file("meta_xstat");
    guard_.add(path);

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        auto s = root.create_stream("Info");
        ASSERT_TRUE(s.has_value());
        auto data = make_test_data(567, 0x77);
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }

    // Stout stat
    {
        auto cf = compound_file::open(path, open_mode::read);
        ASSERT_TRUE(cf.has_value());
        auto children = cf->root_storage().children();
        ASSERT_EQ(children.size(), 1u);
        EXPECT_EQ(children[0].name, "Info");
        EXPECT_EQ(children[0].type, entry_type::stream);
        EXPECT_EQ(children[0].size, 567u);
    }

    // Win32 stat
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    auto entries = win32_enumerate(stg.get());
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].type, STGTY_STREAM);
    EXPECT_EQ(entries[0].cbSize.QuadPart, 567u);
    EXPECT_EQ(std::wstring(entries[0].pwcsName), L"Info");
    free_statstg_name(entries[0]);
}

// ── Win32 creates sub-storage with CLSID, Stout reads ──────────────────
TEST_F(MetadataConformance, Win32SubStorageClsid) {
    auto path = temp_file("meta_subclsid");
    guard_.add(path);

    CLSID sub_clsid;
    ASSERT_TRUE(SUCCEEDED(CoCreateGuid(&sub_clsid)));

    {
        storage_ptr stg;
        ASSERT_TRUE(SUCCEEDED(win32_create_v4(path.wstring(), stg.put())));
        storage_ptr sub;
        ASSERT_TRUE(SUCCEEDED(
            stg->CreateStorage(L"Child", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, sub.put())));
        ASSERT_TRUE(SUCCEEDED(sub->SetClass(sub_clsid)));
    }

    auto cf = compound_file::open(path, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto sub = cf->root_storage().open_storage("Child");
    ASSERT_TRUE(sub.has_value());
    auto st = sub->stat();
    EXPECT_EQ(st.clsid.data1, sub_clsid.Data1);
    EXPECT_EQ(st.clsid.data2, sub_clsid.Data2);
    EXPECT_EQ(st.clsid.data3, sub_clsid.Data3);
    EXPECT_EQ(0, std::memcmp(st.clsid.data4.data(), sub_clsid.Data4, 8));
}

// ── Empty stream size is 0 in both APIs ─────────────────────────────────
TEST_F(MetadataConformance, EmptyStreamSizeZero) {
    auto path = temp_file("meta_empty");
    guard_.add(path);

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("Empty");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }

    // Win32
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"Empty", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 0u);

    // Stout
    auto cf = compound_file::open(path, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto s = cf->root_storage().open_stream("Empty");
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->size(), 0u);
}

// ── Stout sets CLSID, Win32 reads it ────────────────────────────────────
TEST_F(MetadataConformance, StoutClsidWin32Read) {
    auto path = temp_file("meta_sclsid");
    guard_.add(path);

    stout::guid test_id{0x12345678, 0xABCD, 0xEF01, {0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0x01}};
    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        root.set_clsid(test_id);
        ASSERT_TRUE(cf->flush().has_value());
    }

    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    STATSTG stat{};
    ASSERT_TRUE(SUCCEEDED(stg->Stat(&stat, STATFLAG_NONAME)));
    EXPECT_EQ(stat.clsid.Data1, test_id.data1);
    EXPECT_EQ(stat.clsid.Data2, test_id.data2);
    EXPECT_EQ(stat.clsid.Data3, test_id.data3);
    EXPECT_EQ(0, std::memcmp(stat.clsid.Data4, test_id.data4.data(), 8));
}

// ── Stout sets state bits, Win32 reads them ─────────────────────────────
TEST_F(MetadataConformance, StoutStateBitsWin32Read) {
    auto path = temp_file("meta_sbits");
    guard_.add(path);

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        root.set_state_bits(0xCAFEBABE);
        ASSERT_TRUE(cf->flush().has_value());
    }

    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    STATSTG stat{};
    ASSERT_TRUE(SUCCEEDED(stg->Stat(&stat, STATFLAG_NONAME)));
    EXPECT_EQ(stat.grfStateBits, 0xCAFEBABEu);
}

// ── Stout renames storage, Win32 sees new name ──────────────────────────
TEST_F(MetadataConformance, StoutRenameWin32Read) {
    auto path = temp_file("meta_rename");
    guard_.add(path);

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        auto sub = root.create_storage("OldName");
        ASSERT_TRUE(sub.has_value());
        auto rr = sub->rename("NewName");
        ASSERT_TRUE(rr.has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }

    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    auto entries = win32_enumerate(stg.get());
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(std::wstring(entries[0].pwcsName), L"NewName");
    free_statstg_name(entries[0]);
}

// ── Stout sets timestamps, Win32 reads non-zero values ──────────────────
TEST_F(MetadataConformance, StoutTimestampsWin32Read) {
    auto path = temp_file("meta_ts");
    guard_.add(path);

    // Use a known time point: 2020-01-01 00:00:00 UTC
    auto tp = std::chrono::sys_days{std::chrono::year{2020} / std::chrono::January / 1};
    auto ft = stout::file_time{tp};

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        auto sub = root.create_storage("Timed");
        ASSERT_TRUE(sub.has_value());
        sub->set_creation_time(ft);
        sub->set_modified_time(ft);
        ASSERT_TRUE(cf->flush().has_value());
    }

    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    storage_ptr sub;
    ASSERT_TRUE(
        SUCCEEDED(stg->OpenStorage(L"Timed", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0, sub.put())));
    STATSTG stat{};
    ASSERT_TRUE(SUCCEEDED(sub->Stat(&stat, STATFLAG_NONAME)));

    // Both timestamps should be non-zero
    ULARGE_INTEGER ctime, mtime;
    ctime.LowPart = stat.ctime.dwLowDateTime;
    ctime.HighPart = stat.ctime.dwHighDateTime;
    mtime.LowPart = stat.mtime.dwLowDateTime;
    mtime.HighPart = stat.mtime.dwHighDateTime;
    EXPECT_GT(ctime.QuadPart, 0u);
    EXPECT_GT(mtime.QuadPart, 0u);
    // Both should be equal since we set them to the same value
    EXPECT_EQ(ctime.QuadPart, mtime.QuadPart);
}

// ── Stout CLSID roundtrip: set, flush, reopen, read ────────────────────
TEST_F(MetadataConformance, StoutClsidRoundtrip) {
    auto path = temp_file("meta_clsrt");
    guard_.add(path);

    stout::guid test_id{0xAABBCCDD, 0x1122, 0x3344, {0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC}};
    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        cf->root_storage().set_clsid(test_id);
        ASSERT_TRUE(cf->flush().has_value());
    }

    auto cf = compound_file::open(path, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto clsid = cf->root_storage().clsid();
    EXPECT_EQ(clsid.data1, test_id.data1);
    EXPECT_EQ(clsid.data2, test_id.data2);
    EXPECT_EQ(clsid.data3, test_id.data3);
    EXPECT_EQ(clsid.data4, test_id.data4);
}

// ── Stout state bits roundtrip ──────────────────────────────────────────
TEST_F(MetadataConformance, StoutStateBitsRoundtrip) {
    auto path = temp_file("meta_bitsrt");
    guard_.add(path);

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        cf->root_storage().set_state_bits(0x12345678);
        ASSERT_TRUE(cf->flush().has_value());
    }

    auto cf = compound_file::open(path, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    EXPECT_EQ(cf->root_storage().state_bits(), 0x12345678u);
}

#endif // _WIN32
