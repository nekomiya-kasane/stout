#ifdef _WIN32

#include "conformance_utils.h"
#include <stout/compound_file.h>
#include <gtest/gtest.h>

using namespace conformance;
using namespace stout;

class TransactionConformance : public ::testing::Test {
protected:
    com_init com_;
    temp_file_guard guard_;
};

// ── CommitDirect: write in direct mode, close, reopen ───────────────────
TEST_F(TransactionConformance, CommitDirect) {
    auto path = temp_file("txn_direct");
    guard_.add(path);

    auto data = make_test_data(200, 0xAA);
    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("Data");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }

    // Win32 reads
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"Data", nullptr,
        STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 200u);
    std::vector<uint8_t> buf(200);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 200, &rc)));
    EXPECT_EQ(buf, data);
}

// ── TransactedCommit: write in transacted mode, commit, reopen ──────────
TEST_F(TransactionConformance, TransactedCommit) {
    auto path = temp_file("txn_commit");
    guard_.add(path);

    auto data = make_test_data(300, 0xBB);
    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->begin_transaction().has_value());
        auto s = cf->root_storage().create_stream("TxnData");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        ASSERT_TRUE(cf->commit().has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }

    // Win32 reads committed data
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"TxnData", nullptr,
        STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 300u);
    std::vector<uint8_t> buf(300);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 300, &rc)));
    EXPECT_EQ(buf, data);
}

// ── TransactedRevert: write in transacted mode, revert, reopen ──────────
TEST_F(TransactionConformance, TransactedRevert) {
    auto path = temp_file("txn_revert");
    guard_.add(path);

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }

    {
        auto cf = compound_file::open(path, open_mode::read_write);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->begin_transaction().has_value());
        auto s = cf->root_storage().create_stream("Reverted");
        ASSERT_TRUE(s.has_value());
        auto data = make_test_data(100, 0xCC);
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        ASSERT_TRUE(cf->revert().has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }

    // Win32 verifies stream is gone
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    auto entries = win32_enumerate(stg.get());
    EXPECT_EQ(entries.size(), 0u);
    for (auto& e : entries) free_statstg_name(e);
}

// ── TransactedMultipleWrites: multiple writes, single commit ────────────
TEST_F(TransactionConformance, TransactedMultipleWrites) {
    auto path = temp_file("txn_multi");
    guard_.add(path);

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->begin_transaction().has_value());
        auto root = cf->root_storage();
        for (int i = 0; i < 5; ++i) {
            auto s = root.create_stream("S" + std::to_string(i));
            ASSERT_TRUE(s.has_value());
            auto data = make_test_data(100, static_cast<uint8_t>(i));
            ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        }
        ASSERT_TRUE(cf->commit().has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }

    // Win32 reads all 5
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    auto entries = win32_enumerate(stg.get());
    EXPECT_EQ(entries.size(), 5u);
    for (auto& e : entries) free_statstg_name(e);
}

// ── TransactedPartialRevert: commit A, write B, revert → A persists ────
TEST_F(TransactionConformance, TransactedPartialRevert) {
    auto path = temp_file("txn_partial");
    guard_.add(path);

    auto data_a = make_test_data(150, 0xDD);

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());

        // First transaction: create stream A, commit
        ASSERT_TRUE(cf->begin_transaction().has_value());
        auto sa = cf->root_storage().create_stream("StreamA");
        ASSERT_TRUE(sa.has_value());
        ASSERT_TRUE(sa->write(0, std::span<const uint8_t>(data_a)).has_value());
        ASSERT_TRUE(cf->commit().has_value());

        // Second transaction: create stream B, revert
        ASSERT_TRUE(cf->begin_transaction().has_value());
        auto sb = cf->root_storage().create_stream("StreamB");
        ASSERT_TRUE(sb.has_value());
        auto data_b = make_test_data(100, 0xEE);
        ASSERT_TRUE(sb->write(0, std::span<const uint8_t>(data_b)).has_value());
        ASSERT_TRUE(cf->revert().has_value());

        ASSERT_TRUE(cf->flush().has_value());
    }

    // Win32: StreamA exists, StreamB does not
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    auto entries = win32_enumerate(stg.get());
    EXPECT_EQ(entries.size(), 1u);

    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"StreamA", nullptr,
        STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 150u);
    std::vector<uint8_t> buf(150);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 150, &rc)));
    EXPECT_EQ(buf, data_a);

    // StreamB should not exist
    stream_ptr strm_b;
    HRESULT hr = stg->OpenStream(L"StreamB", nullptr,
        STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm_b.put());
    EXPECT_TRUE(FAILED(hr));

    for (auto& e : entries) free_statstg_name(e);
}

// ── TransactedNewStream: create stream in transacted, revert → gone ─────
TEST_F(TransactionConformance, TransactedNewStreamRevert) {
    auto path = temp_file("txn_newstrm");
    guard_.add(path);

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }

    {
        auto cf = compound_file::open(path, open_mode::read_write);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->begin_transaction().has_value());
        auto s = cf->root_storage().create_stream("Ephemeral");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(cf->revert().has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }

    auto cf = compound_file::open(path, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    EXPECT_FALSE(cf->root_storage().exists("Ephemeral"));
}

// ── TransactedDestroyRevert: destroy in transacted, revert → restored ───
TEST_F(TransactionConformance, TransactedDestroyRevert) {
    auto path = temp_file("txn_destroyrev");
    guard_.add(path);

    auto data = make_test_data(200, 0xFF);

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("Keeper");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }

    {
        auto cf = compound_file::open(path, open_mode::read_write);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->begin_transaction().has_value());
        ASSERT_TRUE(cf->root_storage().remove("Keeper").has_value());
        // Verify it's gone within the transaction
        EXPECT_FALSE(cf->root_storage().exists("Keeper"));
        ASSERT_TRUE(cf->revert().has_value());
        // After revert, it should be back
        EXPECT_TRUE(cf->root_storage().exists("Keeper"));
        ASSERT_TRUE(cf->flush().has_value());
    }

    // Win32 verifies stream still exists with correct data
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"Keeper", nullptr,
        STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 200u);
    std::vector<uint8_t> buf(200);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 200, &rc)));
    EXPECT_EQ(buf, data);
}

// ── Win32 transacted commit, Stout reads ────────────────────────────────
TEST_F(TransactionConformance, Win32TransactedStoutRead) {
    auto path = temp_file("txn_w32");
    guard_.add(path);

    auto data = make_test_data(250, 0x44);
    {
        storage_ptr stg;
        // Win32 transacted mode
        STGOPTIONS opts{};
        opts.usVersion = 1;
        opts.ulSectorSize = 4096;
        ASSERT_TRUE(SUCCEEDED(StgCreateStorageEx(
            path.wstring().c_str(),
            STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE | STGM_TRANSACTED,
            STGFMT_DOCFILE, 0, &opts, nullptr,
            IID_IStorage, reinterpret_cast<void**>(stg.put()))));
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(stg->CreateStream(L"TxnStream",
            STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, strm.put())));
        ASSERT_TRUE(SUCCEEDED(win32_stream_write(strm.get(), data.data(), 250)));
        strm.reset();
        ASSERT_TRUE(SUCCEEDED(stg->Commit(STGC_DEFAULT)));
    }

    auto cf = compound_file::open(path, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto s = cf->root_storage().open_stream("TxnStream");
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->size(), 250u);
    std::vector<uint8_t> buf(250);
    ASSERT_TRUE(s->read(0, std::span<uint8_t>(buf)).has_value());
    EXPECT_EQ(buf, data);
}

// ── DoubleBeginFails: can't begin transaction twice ─────────────────────
TEST_F(TransactionConformance, DoubleBeginFails) {
    auto cf = compound_file::create_in_memory(cfb_version::v4);
    ASSERT_TRUE(cf.has_value());
    ASSERT_TRUE(cf->begin_transaction().has_value());
    auto result = cf->begin_transaction();
    EXPECT_FALSE(result.has_value());
}

// ── CommitWithoutTransactionFails ───────────────────────────────────────
TEST_F(TransactionConformance, CommitWithoutTransactionFails) {
    auto cf = compound_file::create_in_memory(cfb_version::v4);
    ASSERT_TRUE(cf.has_value());
    auto result = cf->commit();
    EXPECT_FALSE(result.has_value());
}

// ── RevertWithoutTransactionFails ───────────────────────────────────────
TEST_F(TransactionConformance, RevertWithoutTransactionFails) {
    auto cf = compound_file::create_in_memory(cfb_version::v4);
    ASSERT_TRUE(cf.has_value());
    auto result = cf->revert();
    EXPECT_FALSE(result.has_value());
}

#endif // _WIN32
