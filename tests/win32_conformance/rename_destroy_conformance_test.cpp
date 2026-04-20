#ifdef _WIN32

#include "conformance_utils.h"

#include <gtest/gtest.h>
#include <set>
#include <stout/compound_file.h>

using namespace conformance;
using namespace stout;

class RenameDestroyConformance : public ::testing::Test {
  protected:
    com_init com_;
    temp_file_guard guard_;
};

// ── DestroyStream: create + destroy, verify gone ────────────────────────
TEST_F(RenameDestroyConformance, DestroyStream) {
    auto path = temp_file("rd_dstrm");
    guard_.add(path);

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        auto s = root.create_stream("Temp");
        ASSERT_TRUE(s.has_value());
        auto data = make_test_data(100, 0x42);
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        ASSERT_TRUE(root.remove("Temp").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }

    // Win32 verifies it's gone
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    auto entries = win32_enumerate(stg.get());
    EXPECT_EQ(entries.size(), 0u);
    for (auto &e : entries) free_statstg_name(e);

    // Stout also verifies
    auto cf = compound_file::open(path, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    EXPECT_EQ(cf->root_storage().children().size(), 0u);
}

// ── DestroyStorage: create sub-storage + destroy ────────────────────────
TEST_F(RenameDestroyConformance, DestroyStorage) {
    auto path = temp_file("rd_dstg");
    guard_.add(path);

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        ASSERT_TRUE(root.create_storage("SubDir").has_value());
        ASSERT_TRUE(root.remove("SubDir").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }

    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    auto entries = win32_enumerate(stg.get());
    EXPECT_EQ(entries.size(), 0u);
    for (auto &e : entries) free_statstg_name(e);
}

// ── DestroyNonExistent: destroy something that doesn't exist ────────────
TEST_F(RenameDestroyConformance, DestroyNonExistent) {
    auto path = temp_file("rd_noex");
    guard_.add(path);

    auto cf = compound_file::create(path, cfb_version::v4);
    ASSERT_TRUE(cf.has_value());
    auto result = cf->root_storage().remove("NoSuch");
    EXPECT_FALSE(result.has_value());
}

// ── DestroyWithChildren: destroy storage that has children ──────────────
TEST_F(RenameDestroyConformance, DestroyWithChildren) {
    auto path = temp_file("rd_child");
    guard_.add(path);

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        auto sub = root.create_storage("Parent");
        ASSERT_TRUE(sub.has_value());
        auto s = sub->create_stream("Child");
        ASSERT_TRUE(s.has_value());
        auto data = make_test_data(50, 0xAA);
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        ASSERT_TRUE(root.remove("Parent").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }

    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    auto entries = win32_enumerate(stg.get());
    EXPECT_EQ(entries.size(), 0u);
    for (auto &e : entries) free_statstg_name(e);
}

// ── DestroyAndRecreate: destroy then recreate same name ─────────────────
TEST_F(RenameDestroyConformance, DestroyAndRecreate) {
    auto path = temp_file("rd_recreate");
    guard_.add(path);

    auto data1 = make_test_data(100, 0x11);
    auto data2 = make_test_data(200, 0x22);

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();

        // Create, write, destroy
        auto s1 = root.create_stream("Reused");
        ASSERT_TRUE(s1.has_value());
        ASSERT_TRUE(s1->write(0, std::span<const uint8_t>(data1)).has_value());
        ASSERT_TRUE(root.remove("Reused").has_value());

        // Recreate with different data
        auto s2 = root.create_stream("Reused");
        ASSERT_TRUE(s2.has_value());
        ASSERT_TRUE(s2->write(0, std::span<const uint8_t>(data2)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }

    // Win32 reads the new data
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"Reused", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 200u);
    std::vector<uint8_t> buf(200);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 200, &rc)));
    EXPECT_EQ(buf, data2);
}

// ── DestroyMiddle: create 5, destroy middle one, verify rest ────────────
TEST_F(RenameDestroyConformance, DestroyMiddle) {
    auto path = temp_file("rd_mid");
    guard_.add(path);

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        for (int i = 0; i < 5; ++i) {
            auto s = root.create_stream("S" + std::to_string(i));
            ASSERT_TRUE(s.has_value());
            auto data = make_test_data(50, static_cast<uint8_t>(i));
            ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        }
        ASSERT_TRUE(root.remove("S2").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }

    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    auto entries = win32_enumerate(stg.get());
    EXPECT_EQ(entries.size(), 4u);

    std::set<std::wstring> names;
    for (auto &e : entries) {
        if (e.pwcsName) names.insert(e.pwcsName);
        free_statstg_name(e);
    }
    EXPECT_TRUE(names.count(L"S0"));
    EXPECT_TRUE(names.count(L"S1"));
    EXPECT_FALSE(names.count(L"S2"));
    EXPECT_TRUE(names.count(L"S3"));
    EXPECT_TRUE(names.count(L"S4"));
}

// ── Win32 destroys, Stout reads ─────────────────────────────────────────
TEST_F(RenameDestroyConformance, Win32DestroyStoutRead) {
    auto path = temp_file("rd_w32");
    guard_.add(path);

    // Win32 creates with 3 streams, destroys one
    {
        storage_ptr stg;
        ASSERT_TRUE(SUCCEEDED(win32_create_v4(path.wstring(), stg.put())));
        for (int i = 0; i < 3; ++i) {
            stream_ptr strm;
            auto name = L"Item" + std::to_wstring(i);
            ASSERT_TRUE(SUCCEEDED(stg->CreateStream(name.c_str(), STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE,
                                                    0, 0, strm.put())));
            auto data = make_test_data(80, static_cast<uint8_t>(i));
            ASSERT_TRUE(SUCCEEDED(win32_stream_write(strm.get(), data.data(), 80)));
        }
        ASSERT_TRUE(SUCCEEDED(stg->DestroyElement(L"Item1")));
    }

    // Stout reads
    auto cf = compound_file::open(path, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto children = cf->root_storage().children();
    EXPECT_EQ(children.size(), 2u);

    std::set<std::string> names;
    for (auto &c : children) names.insert(c.name);
    EXPECT_TRUE(names.count("Item0"));
    EXPECT_FALSE(names.count("Item1"));
    EXPECT_TRUE(names.count("Item2"));
}

#endif // _WIN32
