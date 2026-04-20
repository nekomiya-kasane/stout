#include <gtest/gtest.h>
#include "stout/compound_file.h"

using namespace stout;

TEST(TransactionTest, NotInTransactionByDefault) {
    auto cf = compound_file::create_in_memory(cfb_version::v3);
    ASSERT_TRUE(cf.has_value());
    EXPECT_FALSE(cf->in_transaction());
}

TEST(TransactionTest, BeginTransaction) {
    auto cf = compound_file::create_in_memory(cfb_version::v3);
    ASSERT_TRUE(cf.has_value());

    auto r = cf->begin_transaction();
    ASSERT_TRUE(r.has_value());
    EXPECT_TRUE(cf->in_transaction());
}

TEST(TransactionTest, DoubleBeginFails) {
    auto cf = compound_file::create_in_memory(cfb_version::v3);
    ASSERT_TRUE(cf.has_value());

    cf->begin_transaction();
    auto r = cf->begin_transaction();
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), error::transaction_failed);
}

TEST(TransactionTest, CommitWithoutTransactionFails) {
    auto cf = compound_file::create_in_memory(cfb_version::v3);
    ASSERT_TRUE(cf.has_value());

    auto r = cf->commit();
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), error::transaction_failed);
}

TEST(TransactionTest, RevertWithoutTransactionFails) {
    auto cf = compound_file::create_in_memory(cfb_version::v3);
    ASSERT_TRUE(cf.has_value());

    auto r = cf->revert();
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), error::transaction_failed);
}

TEST(TransactionTest, CommitEndsTransaction) {
    auto cf = compound_file::create_in_memory(cfb_version::v3);
    ASSERT_TRUE(cf.has_value());

    cf->begin_transaction();
    EXPECT_TRUE(cf->in_transaction());

    auto r = cf->commit();
    ASSERT_TRUE(r.has_value());
    EXPECT_FALSE(cf->in_transaction());
}

TEST(TransactionTest, RevertEndsTransaction) {
    auto cf = compound_file::create_in_memory(cfb_version::v3);
    ASSERT_TRUE(cf.has_value());

    cf->begin_transaction();
    auto r = cf->revert();
    ASSERT_TRUE(r.has_value());
    EXPECT_FALSE(cf->in_transaction());
}

TEST(TransactionTest, CommitPreservesChanges) {
    auto cf = compound_file::create_in_memory(cfb_version::v3);
    ASSERT_TRUE(cf.has_value());

    cf->begin_transaction();

    auto root = cf->root_storage();
    auto s = root.create_stream("Committed");
    ASSERT_TRUE(s.has_value());
    std::vector<uint8_t> data = {1, 2, 3};
    s->write(0, data);

    cf->commit();

    // Stream should still exist after commit
    auto root2 = cf->root_storage();
    EXPECT_TRUE(root2.exists("Committed"));
    auto s2 = root2.open_stream("Committed");
    ASSERT_TRUE(s2.has_value());
    EXPECT_EQ(s2->size(), 3u);
}

TEST(TransactionTest, RevertUndoesStreamCreation) {
    auto cf = compound_file::create_in_memory(cfb_version::v3);
    ASSERT_TRUE(cf.has_value());

    // Create a stream before the transaction
    auto root = cf->root_storage();
    root.create_stream("Before");
    cf->flush();

    cf->begin_transaction();

    // Create another stream inside the transaction
    auto root2 = cf->root_storage();
    root2.create_stream("During");
    EXPECT_TRUE(root2.exists("During"));

    cf->revert();

    // "During" should be gone, "Before" should remain
    auto root3 = cf->root_storage();
    EXPECT_TRUE(root3.exists("Before"));
    EXPECT_FALSE(root3.exists("During"));
}

TEST(TransactionTest, RevertUndoesStreamWrite) {
    auto cf = compound_file::create_in_memory(cfb_version::v3);
    ASSERT_TRUE(cf.has_value());

    auto root = cf->root_storage();
    auto s = root.create_stream("Data");
    ASSERT_TRUE(s.has_value());
    std::vector<uint8_t> original = {0xAA, 0xBB};
    s->write(0, original);
    cf->flush();

    cf->begin_transaction();

    // Overwrite the stream
    auto s2 = cf->root_storage().open_stream("Data");
    ASSERT_TRUE(s2.has_value());
    std::vector<uint8_t> modified = {0xCC, 0xDD};
    s2->write(0, modified);

    cf->revert();

    // Data should be back to original
    auto s3 = cf->root_storage().open_stream("Data");
    ASSERT_TRUE(s3.has_value());
    EXPECT_EQ(s3->size(), 2u);
    std::vector<uint8_t> buf(2, 0);
    s3->read(0, buf);
    EXPECT_EQ(buf, original);
}

TEST(TransactionTest, RevertUndoesRemoval) {
    auto cf = compound_file::create_in_memory(cfb_version::v3);
    ASSERT_TRUE(cf.has_value());

    auto root = cf->root_storage();
    root.create_stream("Victim");
    cf->flush();

    cf->begin_transaction();
    cf->root_storage().remove("Victim");
    EXPECT_FALSE(cf->root_storage().exists("Victim"));

    cf->revert();
    EXPECT_TRUE(cf->root_storage().exists("Victim"));
}

TEST(TransactionTest, MultipleCommits) {
    auto cf = compound_file::create_in_memory(cfb_version::v3);
    ASSERT_TRUE(cf.has_value());

    // First transaction
    cf->begin_transaction();
    cf->root_storage().create_stream("First");
    cf->commit();

    // Second transaction
    cf->begin_transaction();
    cf->root_storage().create_stream("Second");
    cf->commit();

    auto root = cf->root_storage();
    EXPECT_TRUE(root.exists("First"));
    EXPECT_TRUE(root.exists("Second"));
}
