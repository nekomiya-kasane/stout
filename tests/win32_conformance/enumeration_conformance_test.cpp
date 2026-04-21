#ifdef _WIN32

#include "conformance_utils.h"

#include <gtest/gtest.h>
#include <set>
#include <stout/compound_file.h>

using namespace conformance;
using namespace stout;

class EnumerationConformance : public ::testing::Test {
  protected:
    com_init com_;
    temp_file_guard guard_;
};

// ── EnumEmpty: enumerate root of empty file ─────────────────────────────
TEST_F(EnumerationConformance, EnumEmpty) {
    auto path = temp_file("enum_empty");
    guard_.add(path);

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
    }

    // Win32 enumerates
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    auto entries = win32_enumerate(stg.get());
    EXPECT_EQ(entries.size(), 0u);

    // Stout enumerates
    auto cf = compound_file::open(path, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto children = cf->root_storage().children();
    EXPECT_EQ(children.size(), 0u);
}

// ── EnumStreamsOnly: 5 streams ──────────────────────────────────────────
TEST_F(EnumerationConformance, EnumStreamsOnly) {
    auto path = temp_file("enum_streams");
    guard_.add(path);

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        for (int i = 0; i < 5; ++i) {
            auto s = root.create_stream("Stream" + std::to_string(i));
            ASSERT_TRUE(s.has_value());
            auto data = make_test_data(100, static_cast<uint8_t>(i));
            ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        }
        ASSERT_TRUE(cf->flush().has_value());
    }

    // Win32 enumerates
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    auto entries = win32_enumerate(stg.get());
    EXPECT_EQ(entries.size(), 5u);

    std::set<std::wstring> names;
    for (auto &e : entries) {
        EXPECT_EQ(e.type, STGTY_STREAM);
        if (e.pwcsName) {
            names.insert(e.pwcsName);
        }
        free_statstg_name(e);
    }
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(names.count(L"Stream" + std::to_wstring(i)));
    }
}

// ── EnumStoragesOnly: 5 storages ────────────────────────────────────────
TEST_F(EnumerationConformance, EnumStoragesOnly) {
    auto path = temp_file("enum_storages");
    guard_.add(path);

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        for (int i = 0; i < 5; ++i) {
            ASSERT_TRUE(root.create_storage("Dir" + std::to_string(i)).has_value());
        }
        ASSERT_TRUE(cf->flush().has_value());
    }

    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    auto entries = win32_enumerate(stg.get());
    EXPECT_EQ(entries.size(), 5u);
    for (auto &e : entries) {
        EXPECT_EQ(e.type, STGTY_STORAGE);
        free_statstg_name(e);
    }
}

// ── EnumMixed: 3 storages + 3 streams ───────────────────────────────────
TEST_F(EnumerationConformance, EnumMixed) {
    auto path = temp_file("enum_mixed");
    guard_.add(path);

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        for (int i = 0; i < 3; ++i) {
            ASSERT_TRUE(root.create_storage("Dir" + std::to_string(i)).has_value());
            auto s = root.create_stream("File" + std::to_string(i));
            ASSERT_TRUE(s.has_value());
        }
        ASSERT_TRUE(cf->flush().has_value());
    }

    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    auto entries = win32_enumerate(stg.get());
    EXPECT_EQ(entries.size(), 6u);

    int stg_count = 0, strm_count = 0;
    for (auto &e : entries) {
        if (e.type == STGTY_STORAGE) {
            ++stg_count;
        } else if (e.type == STGTY_STREAM) {
            ++strm_count;
        }
        free_statstg_name(e);
    }
    EXPECT_EQ(stg_count, 3);
    EXPECT_EQ(strm_count, 3);
}

// ── EnumAfterDelete: add 5, delete 2, enumerate ────────────────────────
TEST_F(EnumerationConformance, EnumAfterDelete) {
    auto path = temp_file("enum_del");
    guard_.add(path);

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        for (int i = 0; i < 5; ++i) {
            auto s = root.create_stream("Item" + std::to_string(i));
            ASSERT_TRUE(s.has_value());
            auto data = make_test_data(50, static_cast<uint8_t>(i));
            ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        }
        ASSERT_TRUE(root.remove("Item1").has_value());
        ASSERT_TRUE(root.remove("Item3").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }

    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    auto entries = win32_enumerate(stg.get());
    EXPECT_EQ(entries.size(), 3u);

    std::set<std::wstring> names;
    for (auto &e : entries) {
        if (e.pwcsName) {
            names.insert(e.pwcsName);
        }
        free_statstg_name(e);
    }
    EXPECT_TRUE(names.count(L"Item0"));
    EXPECT_TRUE(names.count(L"Item2"));
    EXPECT_TRUE(names.count(L"Item4"));
    EXPECT_FALSE(names.count(L"Item1"));
    EXPECT_FALSE(names.count(L"Item3"));
}

// ── EnumNestedStorage: enumerate children of a sub-storage ──────────────
TEST_F(EnumerationConformance, EnumNestedStorage) {
    auto path = temp_file("enum_nested");
    guard_.add(path);

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        auto sub = root.create_storage("Sub");
        ASSERT_TRUE(sub.has_value());
        for (int i = 0; i < 3; ++i) {
            auto s = sub->create_stream("Child" + std::to_string(i));
            ASSERT_TRUE(s.has_value());
            auto data = make_test_data(30, static_cast<uint8_t>(i));
            ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        }
        ASSERT_TRUE(cf->flush().has_value());
    }

    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    storage_ptr sub;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStorage(L"Sub", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0, sub.put())));
    auto entries = win32_enumerate(sub.get());
    EXPECT_EQ(entries.size(), 3u);
    for (auto &e : entries) {
        free_statstg_name(e);
    }
}

// ── Win32 creates, Stout enumerates ─────────────────────────────────────
TEST_F(EnumerationConformance, Win32CreateStoutEnum) {
    auto path = temp_file("enum_w32");
    guard_.add(path);

    {
        storage_ptr stg;
        ASSERT_TRUE(SUCCEEDED(win32_create_v4(path.wstring(), stg.put())));
        for (int i = 0; i < 4; ++i) {
            stream_ptr strm;
            auto name = L"Data" + std::to_wstring(i);
            ASSERT_TRUE(SUCCEEDED(stg->CreateStream(name.c_str(), STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE,
                                                    0, 0, strm.put())));
            auto data = make_test_data(80, static_cast<uint8_t>(i));
            ASSERT_TRUE(SUCCEEDED(win32_stream_write(strm.get(), data.data(), 80)));
        }
    }

    auto cf = compound_file::open(path, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto children = cf->root_storage().children();
    EXPECT_EQ(children.size(), 4u);

    std::set<std::string> names;
    for (auto &c : children) {
        names.insert(c.name);
        EXPECT_EQ(c.type, entry_type::stream);
    }
    for (int i = 0; i < 4; ++i) {
        EXPECT_TRUE(names.count("Data" + std::to_string(i)));
    }
}

#endif // _WIN32
