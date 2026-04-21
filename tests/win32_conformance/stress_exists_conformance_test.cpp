#ifdef _WIN32

#    include "conformance_utils.h"

#    include <gtest/gtest.h>
#    include <stout/compound_file.h>

using namespace conformance;
using namespace stout;

struct VPExists {
    cfb_version ver;
    uint16_t major;
};

class StressExistsConformance : public ::testing::TestWithParam<VPExists> {
  protected:
    com_init com_;
    temp_file_guard guard_;
};

static const VPExists vp_exists[] = {{cfb_version::v3, 3}, {cfb_version::v4, 4}};

INSTANTIATE_TEST_SUITE_P(V, StressExistsConformance, ::testing::ValuesIn(vp_exists),
                         [](const auto &info) { return info.param.major == 3 ? "V3" : "V4"; });

// ── exists on stream ────────────────────────────────────────────────────

TEST_P(StressExistsConformance, StreamExists) {
    auto p = temp_file("ex_strm");
    guard_.add(p);
    auto cf = compound_file::create(p, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    ASSERT_TRUE(cf->root_storage().create_stream("A").has_value());
    EXPECT_TRUE(cf->root_storage().exists("A"));
}

TEST_P(StressExistsConformance, StreamNotExists) {
    auto p = temp_file("ex_nostrm");
    guard_.add(p);
    auto cf = compound_file::create(p, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    EXPECT_FALSE(cf->root_storage().exists("A"));
}

// ── exists on storage ───────────────────────────────────────────────────

TEST_P(StressExistsConformance, StorageExists) {
    auto p = temp_file("ex_stg");
    guard_.add(p);
    auto cf = compound_file::create(p, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    ASSERT_TRUE(cf->root_storage().create_storage("D").has_value());
    EXPECT_TRUE(cf->root_storage().exists("D"));
}

TEST_P(StressExistsConformance, StorageNotExists) {
    auto p = temp_file("ex_nostg");
    guard_.add(p);
    auto cf = compound_file::create(p, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    EXPECT_FALSE(cf->root_storage().exists("D"));
}

// ── exists after create and delete ──────────────────────────────────────

TEST_P(StressExistsConformance, ExistsAfterDelete) {
    auto p = temp_file("ex_del");
    guard_.add(p);
    auto cf = compound_file::create(p, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    ASSERT_TRUE(cf->root_storage().create_stream("X").has_value());
    EXPECT_TRUE(cf->root_storage().exists("X"));
    ASSERT_TRUE(cf->root_storage().remove("X").has_value());
    EXPECT_FALSE(cf->root_storage().exists("X"));
}

// ── exists with multiple entries ────────────────────────────────────────

TEST_P(StressExistsConformance, ExistsMultipleEntries) {
    auto p = temp_file("ex_multi");
    guard_.add(p);
    auto cf = compound_file::create(p, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    for (int i = 0; i < 10; ++i) {
        ASSERT_TRUE(cf->root_storage().create_stream("S" + std::to_string(i)).has_value());
    }
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(cf->root_storage().exists("S" + std::to_string(i)));
    }
    EXPECT_FALSE(cf->root_storage().exists("S10"));
}

// ── exists in sub-storage ───────────────────────────────────────────────

TEST_P(StressExistsConformance, ExistsInSubStorage) {
    auto p = temp_file("ex_sub");
    guard_.add(p);
    auto cf = compound_file::create(p, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    auto sub = cf->root_storage().create_storage("Dir");
    ASSERT_TRUE(sub.has_value());
    ASSERT_TRUE(sub->create_stream("Inner").has_value());
    EXPECT_TRUE(sub->exists("Inner"));
    EXPECT_FALSE(sub->exists("Ghost"));
}

// ── exists after reopen ─────────────────────────────────────────────────

TEST_P(StressExistsConformance, ExistsAfterReopen) {
    auto p = temp_file("ex_reopen");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->root_storage().create_stream("Persist").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    EXPECT_TRUE(cf->root_storage().exists("Persist"));
    EXPECT_FALSE(cf->root_storage().exists("Missing"));
}

// ── exists on empty file ────────────────────────────────────────────────

TEST_P(StressExistsConformance, ExistsOnEmptyFile) {
    auto p = temp_file("ex_empty");
    guard_.add(p);
    auto cf = compound_file::create(p, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    EXPECT_FALSE(cf->root_storage().exists("Anything"));
}

// ── exists with mixed types ─────────────────────────────────────────────

TEST_P(StressExistsConformance, ExistsMixedTypes) {
    auto p = temp_file("ex_mixed");
    guard_.add(p);
    auto cf = compound_file::create(p, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    ASSERT_TRUE(cf->root_storage().create_stream("S").has_value());
    ASSERT_TRUE(cf->root_storage().create_storage("D").has_value());
    EXPECT_TRUE(cf->root_storage().exists("S"));
    EXPECT_TRUE(cf->root_storage().exists("D"));
    EXPECT_FALSE(cf->root_storage().exists("X"));
}

// ── exists after rename ─────────────────────────────────────────────────

TEST_P(StressExistsConformance, ExistsAfterRename) {
    auto p = temp_file("ex_rename");
    guard_.add(p);
    auto cf = compound_file::create(p, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    auto s = cf->root_storage().create_stream("Old");
    ASSERT_TRUE(s.has_value());
    ASSERT_TRUE(s->rename("New").has_value());
    EXPECT_FALSE(cf->root_storage().exists("Old"));
    EXPECT_TRUE(cf->root_storage().exists("New"));
}

// ── exists cross-validated with Win32 ───────────────────────────────────

TEST_P(StressExistsConformance, ExistsCrossValidated) {
    auto p = temp_file("ex_cross");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->root_storage().create_stream("A").has_value());
        ASSERT_TRUE(cf->root_storage().create_storage("B").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr sa;
    EXPECT_TRUE(SUCCEEDED(stg->OpenStream(L"A", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, sa.put())));
    storage_ptr sb;
    EXPECT_TRUE(SUCCEEDED(stg->OpenStorage(L"B", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0, sb.put())));
    stream_ptr sc;
    EXPECT_FALSE(SUCCEEDED(stg->OpenStream(L"C", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, sc.put())));
}

// ── exists for 20 entries ───────────────────────────────────────────────

TEST_P(StressExistsConformance, Exists20Entries) {
    auto p = temp_file("ex_20");
    guard_.add(p);
    auto cf = compound_file::create(p, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    for (int i = 0; i < 20; ++i) {
        ASSERT_TRUE(cf->root_storage().create_stream("E" + std::to_string(i)).has_value());
    }
    for (int i = 0; i < 20; ++i) {
        EXPECT_TRUE(cf->root_storage().exists("E" + std::to_string(i)));
    }
    for (int i = 20; i < 25; ++i) {
        EXPECT_FALSE(cf->root_storage().exists("E" + std::to_string(i)));
    }
}

// ── exists after partial delete ─────────────────────────────────────────

TEST_P(StressExistsConformance, ExistsAfterPartialDelete) {
    auto p = temp_file("ex_partdel");
    guard_.add(p);
    auto cf = compound_file::create(p, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    for (int i = 0; i < 10; ++i) {
        ASSERT_TRUE(cf->root_storage().create_stream("S" + std::to_string(i)).has_value());
    }
    for (int i = 0; i < 10; i += 2) {
        ASSERT_TRUE(cf->root_storage().remove("S" + std::to_string(i)).has_value());
    }
    for (int i = 0; i < 10; ++i) {
        if (i % 2 == 0) {
            EXPECT_FALSE(cf->root_storage().exists("S" + std::to_string(i)));
        } else {
            EXPECT_TRUE(cf->root_storage().exists("S" + std::to_string(i)));
        }
    }
}

#endif // _WIN32
