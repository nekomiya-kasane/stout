#ifdef _WIN32

#    include "conformance_utils.h"

#    include <gtest/gtest.h>
#    include <stout/compound_file.h>

using namespace conformance;
using namespace stout;

struct VPMem {
    cfb_version ver;
    uint16_t major;
};

class StressInMemoryConformance : public ::testing::TestWithParam<VPMem> {
  protected:
    com_init com_;
    temp_file_guard guard_;
};

static const VPMem vp_mem[] = {{cfb_version::v3, 3}, {cfb_version::v4, 4}};

INSTANTIATE_TEST_SUITE_P(V, StressInMemoryConformance, ::testing::ValuesIn(vp_mem),
                         [](const auto &info) { return info.param.major == 3 ? "V3" : "V4"; });

// ── Create, flush, reopen, verify ───────────────────────────────────────

TEST_P(StressInMemoryConformance, CreateFlushReopenVerify) {
    auto p = temp_file("im_basic");
    guard_.add(p);
    auto d = make_test_data(1000, 0x11);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("S");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    {
        auto cf = compound_file::open(p, open_mode::read);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().open_stream("S");
        ASSERT_TRUE(s.has_value());
        EXPECT_EQ(s->size(), 1000u);
        std::vector<uint8_t> buf(1000);
        ASSERT_TRUE(s->read(0, std::span<uint8_t>(buf)).has_value());
        EXPECT_EQ(buf, d);
    }
}

// ── Multiple flush cycles ───────────────────────────────────────────────

TEST_P(StressInMemoryConformance, MultipleFlushCycles) {
    auto p = temp_file("im_multiflush");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        for (int i = 0; i < 5; ++i) {
            auto s = cf->root_storage().create_stream("S" + std::to_string(i));
            ASSERT_TRUE(s.has_value());
            auto d = make_test_data(200, static_cast<uint8_t>(i));
            ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
            ASSERT_TRUE(cf->flush().has_value());
        }
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    EXPECT_EQ(cf->root_storage().children().size(), 5u);
}

// ── Read-write mode modifications ───────────────────────────────────────

TEST_P(StressInMemoryConformance, ReadWriteModify) {
    auto p = temp_file("im_rw");
    guard_.add(p);
    auto d1 = make_test_data(500, 0x22);
    auto d2 = make_test_data(300, 0x33);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("S");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d1)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    {
        auto cf = compound_file::open(p, open_mode::read_write);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("T");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d2)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    auto entries = win32_enumerate(stg.get());
    EXPECT_EQ(entries.size(), 2u);
    for (auto &e : entries) {
        free_statstg_name(e);
    }
}

// ── Large number of small streams ───────────────────────────────────────

TEST_P(StressInMemoryConformance, FiftySmallStreams) {
    auto p = temp_file("im_50small");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        for (int i = 0; i < 50; ++i) {
            auto s = cf->root_storage().create_stream("S" + std::to_string(i));
            ASSERT_TRUE(s.has_value());
            auto d = make_test_data(50, static_cast<uint8_t>(i));
            ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        }
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    auto entries = win32_enumerate(stg.get());
    EXPECT_EQ(entries.size(), 50u);
    for (auto &e : entries) {
        free_statstg_name(e);
    }
}

// ── Deep nesting ────────────────────────────────────────────────────────

TEST_P(StressInMemoryConformance, DeepNesting5Levels) {
    auto p = temp_file("im_deep");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto cur = cf->root_storage();
        for (int i = 0; i < 5; ++i) {
            auto sub = cur.create_storage("L" + std::to_string(i));
            ASSERT_TRUE(sub.has_value());
            cur = *sub;
        }
        auto s = cur.create_stream("Leaf");
        ASSERT_TRUE(s.has_value());
        auto d = make_test_data(100, 0x44);
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto l0 = cf->root_storage().open_storage("L0");
    ASSERT_TRUE(l0.has_value());
    auto l1 = l0->open_storage("L1");
    ASSERT_TRUE(l1.has_value());
    auto l2 = l1->open_storage("L2");
    ASSERT_TRUE(l2.has_value());
    auto l3 = l2->open_storage("L3");
    ASSERT_TRUE(l3.has_value());
    auto l4 = l3->open_storage("L4");
    ASSERT_TRUE(l4.has_value());
    auto leaf = l4->open_stream("Leaf");
    ASSERT_TRUE(leaf.has_value());
    EXPECT_EQ(leaf->size(), 100u);
}

// ── Reopen after delete ─────────────────────────────────────────────────

TEST_P(StressInMemoryConformance, ReopenAfterDelete) {
    auto p = temp_file("im_reodel");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->root_storage().create_stream("A").has_value());
        ASSERT_TRUE(cf->root_storage().create_stream("B").has_value());
        ASSERT_TRUE(cf->root_storage().create_stream("C").has_value());
        ASSERT_TRUE(cf->root_storage().remove("B").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto kids = cf->root_storage().children();
    EXPECT_EQ(kids.size(), 2u);
    EXPECT_TRUE(cf->root_storage().exists("A"));
    EXPECT_FALSE(cf->root_storage().exists("B"));
    EXPECT_TRUE(cf->root_storage().exists("C"));
}

// ── Empty storage persists ──────────────────────────────────────────────

TEST_P(StressInMemoryConformance, EmptyStoragePersists) {
    auto p = temp_file("im_emptystg");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->root_storage().create_storage("Empty").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    storage_ptr sub;
    ASSERT_TRUE(
        SUCCEEDED(stg->OpenStorage(L"Empty", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0, sub.put())));
    EXPECT_EQ(win32_enumerate(sub.get()).size(), 0u);
}

// ── Overwrite then reopen ───────────────────────────────────────────────

TEST_P(StressInMemoryConformance, OverwriteThenReopen) {
    auto p = temp_file("im_overwrite");
    guard_.add(p);
    auto d1 = make_test_data(500, 0x55);
    auto d2 = make_test_data(500, 0x66);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("S");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d1)).has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(d2)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto s = cf->root_storage().open_stream("S");
    ASSERT_TRUE(s.has_value());
    std::vector<uint8_t> buf(500);
    ASSERT_TRUE(s->read(0, std::span<uint8_t>(buf)).has_value());
    EXPECT_EQ(buf, d2);
}

// ── Mixed mini and regular after reopen ─────────────────────────────────

TEST_P(StressInMemoryConformance, MixedMiniRegularReopen) {
    auto p = temp_file("im_mixed");
    guard_.add(p);
    auto mini = make_test_data(100, 0x77);
    auto reg = make_test_data(5000, 0x88);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s1 = cf->root_storage().create_stream("Mini");
        ASSERT_TRUE(s1.has_value());
        ASSERT_TRUE(s1->write(0, std::span<const uint8_t>(mini)).has_value());
        auto s2 = cf->root_storage().create_stream("Reg");
        ASSERT_TRUE(s2.has_value());
        ASSERT_TRUE(s2->write(0, std::span<const uint8_t>(reg)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto s1 = cf->root_storage().open_stream("Mini");
    ASSERT_TRUE(s1.has_value());
    EXPECT_EQ(s1->size(), 100u);
    std::vector<uint8_t> b1(100);
    ASSERT_TRUE(s1->read(0, std::span<uint8_t>(b1)).has_value());
    EXPECT_EQ(b1, mini);
    auto s2 = cf->root_storage().open_stream("Reg");
    ASSERT_TRUE(s2.has_value());
    EXPECT_EQ(s2->size(), 5000u);
    std::vector<uint8_t> b2(5000);
    ASSERT_TRUE(s2->read(0, std::span<uint8_t>(b2)).has_value());
    EXPECT_EQ(b2, reg);
}

#endif // _WIN32
