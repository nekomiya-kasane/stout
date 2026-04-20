#ifdef _WIN32

#include "conformance_utils.h"
#include <stout/compound_file.h>
#include <gtest/gtest.h>

using namespace conformance;
using namespace stout;

struct VPConc {
    cfb_version ver;
    uint16_t major;
};

class StressConcurrentConformance : public ::testing::TestWithParam<VPConc> {
protected:
    com_init com_;
    temp_file_guard guard_;
};

static const VPConc vp_conc[] = {{cfb_version::v3, 3}, {cfb_version::v4, 4}};

INSTANTIATE_TEST_SUITE_P(V, StressConcurrentConformance, ::testing::ValuesIn(vp_conc),
    [](const auto& info) { return info.param.major == 3 ? "V3" : "V4"; });

// ── Multiple streams created sequentially ───────────────────────────────

TEST_P(StressConcurrentConformance, TenStreamsSequential) {
    auto p = temp_file("cc_10seq"); guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        for (int i = 0; i < 10; ++i) {
            auto s = cf->root_storage().create_stream("S" + std::to_string(i));
            ASSERT_TRUE(s.has_value());
            auto d = make_test_data(200 + i * 50, static_cast<uint8_t>(i));
            ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        }
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    for (int i = 0; i < 10; ++i) {
        auto name = L"S" + std::to_wstring(i);
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(stg->OpenStream(name.c_str(), nullptr,
            STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
        EXPECT_EQ(win32_stream_size(strm.get()), static_cast<uint64_t>(200 + i * 50));
    }
}

// ── Multiple storages created sequentially ──────────────────────────────

TEST_P(StressConcurrentConformance, TenStoragesSequential) {
    auto p = temp_file("cc_10stg"); guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        for (int i = 0; i < 10; ++i) {
            auto sub = cf->root_storage().create_storage("D" + std::to_string(i));
            ASSERT_TRUE(sub.has_value());
            auto s = sub->create_stream("Inner");
            ASSERT_TRUE(s.has_value());
            auto d = make_test_data(100, static_cast<uint8_t>(i));
            ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        }
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    for (int i = 0; i < 10; ++i) {
        auto name = L"D" + std::to_wstring(i);
        storage_ptr sub;
        ASSERT_TRUE(SUCCEEDED(stg->OpenStorage(name.c_str(), nullptr,
            STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0, sub.put())));
        stream_ptr inner;
        ASSERT_TRUE(SUCCEEDED(sub->OpenStream(L"Inner", nullptr,
            STGM_READ | STGM_SHARE_EXCLUSIVE, 0, inner.put())));
        EXPECT_EQ(win32_stream_size(inner.get()), 100u);
    }
}

// ── Interleaved stream writes ───────────────────────────────────────────

TEST_P(StressConcurrentConformance, InterleavedWrites) {
    auto p = temp_file("cc_interleave"); guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s1 = cf->root_storage().create_stream("A");
        auto s2 = cf->root_storage().create_stream("B");
        ASSERT_TRUE(s1.has_value());
        ASSERT_TRUE(s2.has_value());
        auto d1 = make_test_data(300, 0x11);
        auto d2 = make_test_data(500, 0x22);
        ASSERT_TRUE(s1->write(0, std::span<const uint8_t>(d1)).has_value());
        ASSERT_TRUE(s2->write(0, std::span<const uint8_t>(d2)).has_value());
        // Write more to s1
        auto d3 = make_test_data(200, 0x33);
        ASSERT_TRUE(s1->write(300, std::span<const uint8_t>(d3)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr sa;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"A", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, sa.put())));
    EXPECT_EQ(win32_stream_size(sa.get()), 500u);
    stream_ptr sb;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"B", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, sb.put())));
    EXPECT_EQ(win32_stream_size(sb.get()), 500u);
}

// ── Create and delete in sequence ───────────────────────────────────────

TEST_P(StressConcurrentConformance, CreateDeleteSequence) {
    auto p = temp_file("cc_createdel"); guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        for (int i = 0; i < 20; ++i) {
            auto s = cf->root_storage().create_stream("Temp" + std::to_string(i));
            ASSERT_TRUE(s.has_value());
            auto d = make_test_data(100, static_cast<uint8_t>(i));
            ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        }
        // Delete even-numbered
        for (int i = 0; i < 20; i += 2) {
            ASSERT_TRUE(cf->root_storage().remove("Temp" + std::to_string(i)).has_value());
        }
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    auto entries = win32_enumerate(stg.get());
    EXPECT_EQ(entries.size(), 10u);
    for (auto& e : entries) free_statstg_name(e);
}

// ── Rapid open/close cycles ─────────────────────────────────────────────

TEST_P(StressConcurrentConformance, RapidOpenClose) {
    auto p = temp_file("cc_rapid"); guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("Data");
        ASSERT_TRUE(s.has_value());
        auto d = make_test_data(1000, 0x55);
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    // Open and close 10 times
    for (int i = 0; i < 10; ++i) {
        auto cf = compound_file::open(p, open_mode::read);
        ASSERT_TRUE(cf.has_value()) << "Iteration " << i;
        auto s = cf->root_storage().open_stream("Data");
        ASSERT_TRUE(s.has_value()) << "Iteration " << i;
        EXPECT_EQ(s->size(), 1000u);
    }
}

// ── Multiple files created independently ────────────────────────────────

TEST_P(StressConcurrentConformance, MultipleIndependentFiles) {
    std::vector<std::filesystem::path> paths;
    for (int i = 0; i < 5; ++i) {
        auto p = temp_file("cc_indep" + std::to_string(i));
        guard_.add(p);
        paths.push_back(p);
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("S");
        ASSERT_TRUE(s.has_value());
        auto d = make_test_data(100 * (i + 1), static_cast<uint8_t>(i));
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    for (int i = 0; i < 5; ++i) {
        storage_ptr stg;
        ASSERT_TRUE(SUCCEEDED(win32_open_read(paths[i].wstring(), stg.put())));
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"S", nullptr,
            STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
        EXPECT_EQ(win32_stream_size(strm.get()), static_cast<uint64_t>(100 * (i + 1)));
    }
}

// ── Write then read same session ────────────────────────────────────────

TEST_P(StressConcurrentConformance, WriteReadSameSession) {
    auto p = temp_file("cc_wrrd"); guard_.add(p);
    auto cf = compound_file::create(p, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    for (int i = 0; i < 10; ++i) {
        auto s = cf->root_storage().create_stream("S" + std::to_string(i));
        ASSERT_TRUE(s.has_value());
        auto d = make_test_data(200, static_cast<uint8_t>(i));
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
    }
    // Read back in same session
    for (int i = 0; i < 10; ++i) {
        auto s = cf->root_storage().open_stream("S" + std::to_string(i));
        ASSERT_TRUE(s.has_value());
        EXPECT_EQ(s->size(), 200u);
        std::vector<uint8_t> buf(200);
        auto rd = s->read(0, std::span<uint8_t>(buf));
        ASSERT_TRUE(rd.has_value());
        EXPECT_EQ(buf, make_test_data(200, static_cast<uint8_t>(i)));
    }
}

// ── Overwrite existing stream data ──────────────────────────────────────

TEST_P(StressConcurrentConformance, OverwriteStreamData) {
    auto p = temp_file("cc_overwrite"); guard_.add(p);
    auto orig = make_test_data(500, 0xAA);
    auto repl = make_test_data(500, 0xBB);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("S");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(orig)).has_value());
        // Overwrite with different data
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(repl)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"S", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    std::vector<uint8_t> buf(500);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 500, &rc)));
    EXPECT_EQ(buf, repl);
}

// ── Children count after mixed operations ───────────────────────────────

TEST_P(StressConcurrentConformance, ChildrenCountAfterMixedOps) {
    auto p = temp_file("cc_mixed"); guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        ASSERT_TRUE(root.create_stream("A").has_value());
        ASSERT_TRUE(root.create_stream("B").has_value());
        ASSERT_TRUE(root.create_stream("C").has_value());
        ASSERT_TRUE(root.create_storage("D").has_value());
        ASSERT_TRUE(root.remove("B").has_value());
        ASSERT_TRUE(root.create_stream("E").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto kids = cf->root_storage().children();
    EXPECT_EQ(kids.size(), 4u); // A, C, D, E
}

#endif // _WIN32
