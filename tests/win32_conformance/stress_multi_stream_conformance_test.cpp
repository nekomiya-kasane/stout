#ifdef _WIN32

#include "conformance_utils.h"

#include <gtest/gtest.h>
#include <stout/compound_file.h>

using namespace conformance;
using namespace stout;

struct VPMulti {
    cfb_version ver;
    uint16_t major;
};

class StressMultiStreamConformance : public ::testing::TestWithParam<VPMulti> {
  protected:
    com_init com_;
    temp_file_guard guard_;
};

static const VPMulti vp_multi[] = {{cfb_version::v3, 3}, {cfb_version::v4, 4}};

INSTANTIATE_TEST_SUITE_P(V, StressMultiStreamConformance, ::testing::ValuesIn(vp_multi),
                         [](const auto &info) { return info.param.major == 3 ? "V3" : "V4"; });

// ── 20 streams with varying sizes ───────────────────────────────────────

TEST_P(StressMultiStreamConformance, TwentyVaryingSizes) {
    auto p = temp_file("ms_20var");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        for (int i = 0; i < 20; ++i) {
            auto s = cf->root_storage().create_stream("V" + std::to_string(i));
            ASSERT_TRUE(s.has_value());
            auto d = make_test_data(10 + i * 100, static_cast<uint8_t>(i));
            ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        }
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    for (int i = 0; i < 20; ++i) {
        auto name = L"V" + std::to_wstring(i);
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(stg->OpenStream(name.c_str(), nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
        EXPECT_EQ(win32_stream_size(strm.get()), static_cast<uint64_t>(10 + i * 100));
    }
}

// ── 20 streams all same size ────────────────────────────────────────────

TEST_P(StressMultiStreamConformance, TwentySameSize) {
    auto p = temp_file("ms_20same");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        for (int i = 0; i < 20; ++i) {
            auto s = cf->root_storage().create_stream("S" + std::to_string(i));
            ASSERT_TRUE(s.has_value());
            auto d = make_test_data(256, static_cast<uint8_t>(i));
            ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        }
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    for (int i = 0; i < 20; ++i) {
        auto name = L"S" + std::to_wstring(i);
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(stg->OpenStream(name.c_str(), nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
        EXPECT_EQ(win32_stream_size(strm.get()), 256u);
        std::vector<uint8_t> buf(256);
        ULONG rc = 0;
        ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 256, &rc)));
        EXPECT_EQ(buf, make_test_data(256, static_cast<uint8_t>(i)));
    }
}

// ── All mini streams ────────────────────────────────────────────────────

TEST_P(StressMultiStreamConformance, TwentyMiniStreams) {
    auto p = temp_file("ms_20mini");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        for (int i = 0; i < 20; ++i) {
            auto s = cf->root_storage().create_stream("M" + std::to_string(i));
            ASSERT_TRUE(s.has_value());
            auto d = make_test_data(50 + i * 10, static_cast<uint8_t>(i + 0x10));
            ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        }
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    for (int i = 0; i < 20; ++i) {
        auto name = L"M" + std::to_wstring(i);
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(stg->OpenStream(name.c_str(), nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
        EXPECT_EQ(win32_stream_size(strm.get()), static_cast<uint64_t>(50 + i * 10));
    }
}

// ── All regular streams ─────────────────────────────────────────────────

TEST_P(StressMultiStreamConformance, FiveRegularStreams) {
    auto p = temp_file("ms_5reg");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        for (int i = 0; i < 5; ++i) {
            auto s = cf->root_storage().create_stream("R" + std::to_string(i));
            ASSERT_TRUE(s.has_value());
            auto d = make_test_data(4200 + i * 200, static_cast<uint8_t>(i + 0x20));
            ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        }
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    for (int i = 0; i < 5; ++i) {
        auto name = L"R" + std::to_wstring(i);
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(stg->OpenStream(name.c_str(), nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
        EXPECT_EQ(win32_stream_size(strm.get()), static_cast<uint64_t>(4200 + i * 200));
    }
}

// ── Mixed mini and regular ──────────────────────────────────────────────

TEST_P(StressMultiStreamConformance, MixedMiniRegular15) {
    auto p = temp_file("ms_mixed15");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        for (int i = 0; i < 15; ++i) {
            auto s = cf->root_storage().create_stream("X" + std::to_string(i));
            ASSERT_TRUE(s.has_value());
            uint32_t sz = (i % 2 == 0) ? (100 + i * 20) : (5000 + i * 100);
            auto d = make_test_data(sz, static_cast<uint8_t>(i + 0x30));
            ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        }
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    for (int i = 0; i < 15; ++i) {
        auto name = L"X" + std::to_wstring(i);
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(stg->OpenStream(name.c_str(), nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
        uint32_t sz = (i % 2 == 0) ? (100 + i * 20) : (5000 + i * 100);
        EXPECT_EQ(win32_stream_size(strm.get()), static_cast<uint64_t>(sz));
    }
}

// ── Streams in sub-storages ─────────────────────────────────────────────

TEST_P(StressMultiStreamConformance, StreamsInSubStorages) {
    auto p = temp_file("ms_substg");
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
                auto d = make_test_data(200, static_cast<uint8_t>(i * 4 + j));
                ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
            }
        }
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    for (int i = 0; i < 5; ++i) {
        auto dname = L"D" + std::to_wstring(i);
        storage_ptr sub;
        ASSERT_TRUE(SUCCEEDED(
            stg->OpenStorage(dname.c_str(), nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0, sub.put())));
        for (int j = 0; j < 4; ++j) {
            auto sname = L"S" + std::to_wstring(j);
            stream_ptr strm;
            ASSERT_TRUE(
                SUCCEEDED(sub->OpenStream(sname.c_str(), nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
            EXPECT_EQ(win32_stream_size(strm.get()), 200u);
        }
    }
}

// ── Data integrity for all streams ──────────────────────────────────────

TEST_P(StressMultiStreamConformance, DataIntegrity10Streams) {
    auto p = temp_file("ms_integ");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        for (int i = 0; i < 10; ++i) {
            auto s = cf->root_storage().create_stream("I" + std::to_string(i));
            ASSERT_TRUE(s.has_value());
            auto d = make_test_data(300, static_cast<uint8_t>(i + 0x40));
            ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        }
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    for (int i = 0; i < 10; ++i) {
        auto name = L"I" + std::to_wstring(i);
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(stg->OpenStream(name.c_str(), nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
        std::vector<uint8_t> buf(300);
        ULONG rc = 0;
        ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 300, &rc)));
        EXPECT_EQ(buf, make_test_data(300, static_cast<uint8_t>(i + 0x40)));
    }
}

// ── Win32 creates many, Stout reads ─────────────────────────────────────

TEST_P(StressMultiStreamConformance, Win32Creates15StoutReads) {
    auto p = temp_file("ms_w32_15");
    guard_.add(p);
    {
        storage_ptr stg;
        if (GetParam().ver == cfb_version::v4)
            ASSERT_TRUE(SUCCEEDED(win32_create_v4(p.wstring(), stg.put())));
        else
            ASSERT_TRUE(SUCCEEDED(win32_create_v3(p.wstring(), stg.put())));
        for (int i = 0; i < 15; ++i) {
            auto name = L"W" + std::to_wstring(i);
            stream_ptr strm;
            ASSERT_TRUE(SUCCEEDED(stg->CreateStream(name.c_str(), STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE,
                                                    0, 0, strm.put())));
            auto d = make_test_data(150 + i * 50, static_cast<uint8_t>(i + 0x50));
            ASSERT_TRUE(SUCCEEDED(win32_stream_write(strm.get(), d.data(), static_cast<ULONG>(d.size()))));
        }
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto kids = cf->root_storage().children();
    EXPECT_EQ(kids.size(), 15u);
    for (int i = 0; i < 15; ++i) {
        auto s = cf->root_storage().open_stream("W" + std::to_string(i));
        ASSERT_TRUE(s.has_value());
        EXPECT_EQ(s->size(), static_cast<uint64_t>(150 + i * 50));
    }
}

// ── Empty streams mixed with data streams ───────────────────────────────

TEST_P(StressMultiStreamConformance, EmptyAndDataMixed) {
    auto p = temp_file("ms_emptydata");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        for (int i = 0; i < 10; ++i) {
            auto s = cf->root_storage().create_stream("E" + std::to_string(i));
            ASSERT_TRUE(s.has_value());
            if (i % 2 == 0) {
                auto d = make_test_data(500, static_cast<uint8_t>(i));
                ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
            }
        }
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    for (int i = 0; i < 10; ++i) {
        auto name = L"E" + std::to_wstring(i);
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(stg->OpenStream(name.c_str(), nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
        if (i % 2 == 0)
            EXPECT_EQ(win32_stream_size(strm.get()), 500u);
        else
            EXPECT_EQ(win32_stream_size(strm.get()), 0u);
    }
}

// ── Storages mixed with streams at root ─────────────────────────────────

TEST_P(StressMultiStreamConformance, StoragesAndStreamsAtRoot) {
    auto p = temp_file("ms_rootmix");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        for (int i = 0; i < 10; ++i) {
            if (i % 3 == 0) {
                auto sub = cf->root_storage().create_storage("D" + std::to_string(i));
                ASSERT_TRUE(sub.has_value());
            } else {
                auto s = cf->root_storage().create_stream("S" + std::to_string(i));
                ASSERT_TRUE(s.has_value());
                auto d = make_test_data(100, static_cast<uint8_t>(i));
                ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
            }
        }
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    auto entries = win32_enumerate(stg.get());
    EXPECT_EQ(entries.size(), 10u);
    for (auto &e : entries) free_statstg_name(e);
}

#endif // _WIN32
