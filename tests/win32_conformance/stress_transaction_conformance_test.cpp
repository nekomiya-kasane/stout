#ifdef _WIN32

#include "conformance_utils.h"

#include <gtest/gtest.h>
#include <stout/compound_file.h>

using namespace conformance;
using namespace stout;

struct VPTxn {
    cfb_version ver;
    uint16_t major;
};

class StressTransactionConformance : public ::testing::TestWithParam<VPTxn> {
  protected:
    com_init com_;
    temp_file_guard guard_;
};

static const VPTxn vp_txn[] = {{cfb_version::v3, 3}, {cfb_version::v4, 4}};

INSTANTIATE_TEST_SUITE_P(V, StressTransactionConformance, ::testing::ValuesIn(vp_txn),
                         [](const auto &info) { return info.param.major == 3 ? "V3" : "V4"; });

// ── Commit preserves data ───────────────────────────────────────────────

TEST_P(StressTransactionConformance, CommitPreservesStream) {
    auto p = temp_file("st_commit");
    guard_.add(p);
    auto data = make_test_data(300, 0x11);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->begin_transaction().has_value());
        auto s = cf->root_storage().create_stream("D");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        ASSERT_TRUE(cf->commit().has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto s = cf->root_storage().open_stream("D");
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->size(), 300u);
}

TEST_P(StressTransactionConformance, CommitPreservesStorage) {
    auto p = temp_file("st_cstg");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->begin_transaction().has_value());
        ASSERT_TRUE(cf->root_storage().create_storage("Dir").has_value());
        ASSERT_TRUE(cf->commit().has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    EXPECT_TRUE(cf->root_storage().exists("Dir"));
}

// ── Revert discards changes ─────────────────────────────────────────────

TEST_P(StressTransactionConformance, RevertDiscardsStream) {
    auto p = temp_file("st_revert");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->flush().has_value());
        ASSERT_TRUE(cf->begin_transaction().has_value());
        auto s = cf->root_storage().create_stream("Ghost");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(cf->revert().has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    EXPECT_EQ(cf->root_storage().children().size(), 0u);
}

TEST_P(StressTransactionConformance, RevertDiscardsStorage) {
    auto p = temp_file("st_rvstg");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->flush().has_value());
        ASSERT_TRUE(cf->begin_transaction().has_value());
        ASSERT_TRUE(cf->root_storage().create_storage("Ghost").has_value());
        ASSERT_TRUE(cf->revert().has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    EXPECT_EQ(cf->root_storage().children().size(), 0u);
}

// ── Partial commit + revert ─────────────────────────────────────────────

TEST_P(StressTransactionConformance, PartialCommitRevert) {
    auto p = temp_file("st_partial");
    guard_.add(p);
    auto data = make_test_data(200, 0x22);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        // First transaction: create and commit
        ASSERT_TRUE(cf->begin_transaction().has_value());
        auto s = cf->root_storage().create_stream("Keep");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        ASSERT_TRUE(cf->commit().has_value());
        // Second transaction: create and revert
        ASSERT_TRUE(cf->begin_transaction().has_value());
        ASSERT_TRUE(cf->root_storage().create_stream("Discard").has_value());
        ASSERT_TRUE(cf->revert().has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    EXPECT_TRUE(cf->root_storage().exists("Keep"));
    EXPECT_FALSE(cf->root_storage().exists("Discard"));
}

// ── Transaction with multiple operations ────────────────────────────────

TEST_P(StressTransactionConformance, MultipleOpsInTransaction) {
    auto p = temp_file("st_multi");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->begin_transaction().has_value());
        auto root = cf->root_storage();
        ASSERT_TRUE(root.create_stream("S1").has_value());
        ASSERT_TRUE(root.create_stream("S2").has_value());
        ASSERT_TRUE(root.create_storage("D1").has_value());
        ASSERT_TRUE(cf->commit().has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    EXPECT_EQ(cf->root_storage().children().size(), 3u);
}

// ── Transaction with delete ─────────────────────────────────────────────

TEST_P(StressTransactionConformance, TransactionDeleteRevert) {
    auto p = temp_file("st_delrev");
    guard_.add(p);
    auto data = make_test_data(100, 0x33);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("Victim");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
        // Delete in transaction, then revert
        ASSERT_TRUE(cf->begin_transaction().has_value());
        ASSERT_TRUE(cf->root_storage().remove("Victim").has_value());
        ASSERT_TRUE(cf->revert().has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    EXPECT_TRUE(cf->root_storage().exists("Victim"));
}

TEST_P(StressTransactionConformance, TransactionDeleteCommit) {
    auto p = temp_file("st_delcom");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->root_storage().create_stream("Victim").has_value());
        ASSERT_TRUE(cf->flush().has_value());
        ASSERT_TRUE(cf->begin_transaction().has_value());
        ASSERT_TRUE(cf->root_storage().remove("Victim").has_value());
        ASSERT_TRUE(cf->commit().has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    EXPECT_FALSE(cf->root_storage().exists("Victim"));
}

// ── Transaction with resize ─────────────────────────────────────────────

TEST_P(StressTransactionConformance, TransactionResizeCommit) {
    auto p = temp_file("st_rsz");
    guard_.add(p);
    auto data = make_test_data(100, 0x44);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("D");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
        ASSERT_TRUE(cf->begin_transaction().has_value());
        auto s2 = cf->root_storage().open_stream("D");
        ASSERT_TRUE(s2.has_value());
        ASSERT_TRUE(s2->resize(5000).has_value());
        ASSERT_TRUE(cf->commit().has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto s = cf->root_storage().open_stream("D");
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->size(), 5000u);
}

TEST_P(StressTransactionConformance, TransactionResizeRevert) {
    auto p = temp_file("st_rszrev");
    guard_.add(p);
    auto data = make_test_data(100, 0x55);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("D");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
        ASSERT_TRUE(cf->begin_transaction().has_value());
        auto s2 = cf->root_storage().open_stream("D");
        ASSERT_TRUE(s2.has_value());
        ASSERT_TRUE(s2->resize(5000).has_value());
        ASSERT_TRUE(cf->revert().has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto s = cf->root_storage().open_stream("D");
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->size(), 100u);
}

// ── Transaction + Win32 cross-read ──────────────────────────────────────

TEST_P(StressTransactionConformance, CommitThenWin32Read) {
    auto p = temp_file("st_w32");
    guard_.add(p);
    auto data = make_test_data(500, 0x66);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->begin_transaction().has_value());
        auto s = cf->root_storage().create_stream("D");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        ASSERT_TRUE(cf->commit().has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"D", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 500u);
}

// ── Double begin fails ──────────────────────────────────────────────────

TEST_P(StressTransactionConformance, DoubleBeginFails) {
    auto p = temp_file("st_dbl");
    guard_.add(p);
    auto cf = compound_file::create(p, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    ASSERT_TRUE(cf->begin_transaction().has_value());
    auto r = cf->begin_transaction();
    EXPECT_FALSE(r.has_value());
}

// ── Commit/revert without transaction ───────────────────────────────────

TEST_P(StressTransactionConformance, CommitWithoutTransactionFails) {
    auto p = temp_file("st_nocom");
    guard_.add(p);
    auto cf = compound_file::create(p, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    auto r = cf->commit();
    EXPECT_FALSE(r.has_value());
}

TEST_P(StressTransactionConformance, RevertWithoutTransactionFails) {
    auto p = temp_file("st_norev");
    guard_.add(p);
    auto cf = compound_file::create(p, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    auto r = cf->revert();
    EXPECT_FALSE(r.has_value());
}

// ── in_transaction query ────────────────────────────────────────────────

TEST_P(StressTransactionConformance, InTransactionQuery) {
    auto p = temp_file("st_query");
    guard_.add(p);
    auto cf = compound_file::create(p, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    EXPECT_FALSE(cf->in_transaction());
    ASSERT_TRUE(cf->begin_transaction().has_value());
    EXPECT_TRUE(cf->in_transaction());
    ASSERT_TRUE(cf->commit().has_value());
    EXPECT_FALSE(cf->in_transaction());
}

// ── Transaction with metadata ───────────────────────────────────────────

TEST_P(StressTransactionConformance, TransactionMetadataCommit) {
    auto p = temp_file("st_meta");
    guard_.add(p);
    stout::guid id{0xABCDEF01, 0x2345, 0x6789, {1, 2, 3, 4, 5, 6, 7, 8}};
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->flush().has_value());
        ASSERT_TRUE(cf->begin_transaction().has_value());
        cf->root_storage().set_clsid(id);
        cf->root_storage().set_state_bits(0x12345678);
        ASSERT_TRUE(cf->commit().has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    EXPECT_EQ(cf->root_storage().clsid(), id);
    EXPECT_EQ(cf->root_storage().state_bits(), 0x12345678u);
}

TEST_P(StressTransactionConformance, TransactionMetadataRevert) {
    auto p = temp_file("st_metarev");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->flush().has_value());
        ASSERT_TRUE(cf->begin_transaction().has_value());
        cf->root_storage().set_state_bits(0xDEADBEEF);
        ASSERT_TRUE(cf->revert().has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    EXPECT_EQ(cf->root_storage().state_bits(), 0u);
}

#endif // _WIN32
