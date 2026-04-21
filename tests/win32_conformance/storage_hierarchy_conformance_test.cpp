#ifdef _WIN32

#    include "conformance_utils.h"

#    include <algorithm>
#    include <gtest/gtest.h>
#    include <set>
#    include <stout/compound_file.h>

using namespace conformance;
using namespace stout;

class StorageHierarchyConformance : public ::testing::Test {
  protected:
    com_init com_;
    temp_file_guard guard_;
};

// ── SingleSubStorage: Stout creates, Win32 opens ────────────────────────
TEST_F(StorageHierarchyConformance, SingleSubStorage) {
    auto path = temp_file("sub_stg");
    guard_.add(path);

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto sub = cf->root_storage().create_storage("Sub1");
        ASSERT_TRUE(sub.has_value()) << error_message(sub.error());
        ASSERT_TRUE(cf->flush().has_value());
    }

    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    storage_ptr sub;
    HRESULT hr = stg->OpenStorage(L"Sub1", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0, sub.put());
    ASSERT_TRUE(SUCCEEDED(hr)) << "Win32 failed to open Sub1, hr=0x" << std::hex << hr;
}

// ── NestedThreeLevels: root → A → B → C ────────────────────────────────
TEST_F(StorageHierarchyConformance, NestedThreeLevels) {
    auto path = temp_file("nested3");
    guard_.add(path);

    {
        auto cf = compound_file::create(path, cfb_version::v4);
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
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    storage_ptr a, b, c;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStorage(L"A", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0, a.put())));
    ASSERT_TRUE(SUCCEEDED(a->OpenStorage(L"B", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0, b.put())));
    ASSERT_TRUE(SUCCEEDED(b->OpenStorage(L"C", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0, c.put())));
}

// ── StorageAndStream: root has both sub-storage and stream ──────────────
TEST_F(StorageHierarchyConformance, StorageAndStream) {
    auto path = temp_file("stg_strm");
    guard_.add(path);

    auto data = make_test_data(50);
    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        ASSERT_TRUE(root.create_storage("SubStorage").has_value());
        auto s = root.create_stream("DataStream");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }

    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));

    // Open sub-storage
    storage_ptr sub;
    ASSERT_TRUE(
        SUCCEEDED(stg->OpenStorage(L"SubStorage", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0, sub.put())));

    // Open stream
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"DataStream", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 50u);
}

// ── TenSiblings: 10 storages at same level ──────────────────────────────
TEST_F(StorageHierarchyConformance, TenSiblings) {
    auto path = temp_file("ten_sibs");
    guard_.add(path);

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        for (int i = 0; i < 10; ++i) {
            auto name = "Storage" + std::to_string(i);
            ASSERT_TRUE(root.create_storage(name).has_value()) << "Failed: " << name;
        }
        ASSERT_TRUE(cf->flush().has_value());
    }

    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));

    // Enumerate and verify all 10 exist
    auto entries = win32_enumerate(stg.get());
    std::set<std::wstring> names;
    for (auto &e : entries) {
        if (e.pwcsName) {
            names.insert(e.pwcsName);
            free_statstg_name(e);
        }
    }
    EXPECT_EQ(names.size(), 10u);
    for (int i = 0; i < 10; ++i) {
        auto name = L"Storage" + std::to_wstring(i);
        EXPECT_TRUE(names.count(name)) << "Missing: Storage" << i;
    }
}

// ── MixedChildren: 5 storages + 5 streams ───────────────────────────────
TEST_F(StorageHierarchyConformance, MixedChildren) {
    auto path = temp_file("mixed");
    guard_.add(path);

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        for (int i = 0; i < 5; ++i) {
            ASSERT_TRUE(root.create_storage("Dir" + std::to_string(i)).has_value());
            auto s = root.create_stream("File" + std::to_string(i));
            ASSERT_TRUE(s.has_value());
            auto data = make_test_data(64, static_cast<uint8_t>(i));
            ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        }
        ASSERT_TRUE(cf->flush().has_value());
    }

    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    auto entries = win32_enumerate(stg.get());
    EXPECT_EQ(entries.size(), 10u);

    int storage_count = 0, stream_count = 0;
    for (auto &e : entries) {
        if (e.type == STGTY_STORAGE) {
            ++storage_count;
        } else if (e.type == STGTY_STREAM) {
            ++stream_count;
        }
        free_statstg_name(e);
    }
    EXPECT_EQ(storage_count, 5);
    EXPECT_EQ(stream_count, 5);
}

// ── DeepNesting: 10 levels, each with 1 stream ─────────────────────────
TEST_F(StorageHierarchyConformance, DeepNesting) {
    auto path = temp_file("deep10");
    guard_.add(path);

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        storage current = cf->root_storage();
        for (int i = 0; i < 10; ++i) {
            auto name = "Level" + std::to_string(i);
            auto data = make_test_data(32, static_cast<uint8_t>(i));
            auto s = current.create_stream("Data" + std::to_string(i));
            ASSERT_TRUE(s.has_value());
            ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
            auto sub = current.create_storage(name);
            ASSERT_TRUE(sub.has_value());
            current = std::move(*sub);
        }
        ASSERT_TRUE(cf->flush().has_value());
    }

    // Win32 traverses all 10 levels
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    IStorage *cur = stg.get();
    std::vector<storage_ptr> holders; // keep alive
    for (int i = 0; i < 10; ++i) {
        // Read stream at this level
        auto strm_name = L"Data" + std::to_wstring(i);
        stream_ptr strm;
        ASSERT_TRUE(
            SUCCEEDED(cur->OpenStream(strm_name.c_str(), nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())))
            << "Failed to open Data" << i;
        EXPECT_EQ(win32_stream_size(strm.get()), 32u);

        // Open next level storage
        auto stg_name = L"Level" + std::to_wstring(i);
        holders.emplace_back();
        ASSERT_TRUE(SUCCEEDED(cur->OpenStorage(stg_name.c_str(), nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0,
                                               holders.back().put())))
            << "Failed to open Level" << i;
        cur = holders.back().get();
    }
}

// ── Win32 creates hierarchy, Stout reads ────────────────────────────────
TEST_F(StorageHierarchyConformance, Win32CreateStoutRead) {
    auto path = temp_file("w32_hier");
    guard_.add(path);

    // Win32 creates: root -> SubA (storage) + DataStream (stream)
    {
        storage_ptr stg;
        ASSERT_TRUE(SUCCEEDED(win32_create_v4(path.wstring(), stg.put())));
        storage_ptr sub;
        ASSERT_TRUE(SUCCEEDED(
            stg->CreateStorage(L"SubA", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, sub.put())));
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(
            stg->CreateStream(L"DataStream", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, strm.put())));
        auto data = make_test_data(200);
        ASSERT_TRUE(SUCCEEDED(win32_stream_write(strm.get(), data.data(), 200)));
    }

    // Stout reads
    auto cf = compound_file::open(path, open_mode::read);
    ASSERT_TRUE(cf.has_value()) << error_message(cf.error());
    auto root = cf->root_storage();

    // Check children
    auto children = root.children();
    EXPECT_EQ(children.size(), 2u);

    // Open sub-storage
    auto sub = root.open_storage("SubA");
    ASSERT_TRUE(sub.has_value()) << error_message(sub.error());

    // Open and read stream
    auto s = root.open_stream("DataStream");
    ASSERT_TRUE(s.has_value()) << error_message(s.error());
    EXPECT_EQ(s->size(), 200u);
    std::vector<uint8_t> buf(200);
    auto rd = s->read(0, std::span<uint8_t>(buf));
    ASSERT_TRUE(rd.has_value());
    EXPECT_EQ(buf, make_test_data(200));
}

#endif // _WIN32
