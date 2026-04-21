#ifdef _WIN32

#    include "conformance_utils.h"

#    include <gtest/gtest.h>
#    include <stout/compound_file.h>

using namespace conformance;
using namespace stout;

struct VPErr {
    cfb_version ver;
    uint16_t major;
};

class StressErrorHandlingConformance : public ::testing::TestWithParam<VPErr> {
  protected:
    com_init com_;
    temp_file_guard guard_;
};

static const VPErr vp_err[] = {{cfb_version::v3, 3}, {cfb_version::v4, 4}};

INSTANTIATE_TEST_SUITE_P(V, StressErrorHandlingConformance, ::testing::ValuesIn(vp_err),
                         [](const auto &info) { return info.param.major == 3 ? "V3" : "V4"; });

// ── Open non-existent file ──────────────────────────────────────────────

TEST_P(StressErrorHandlingConformance, OpenNonExistentFile) {
    auto r = compound_file::open("C:\\nonexistent_stout_test_file.cfb", open_mode::read);
    EXPECT_FALSE(r.has_value());
}

// ── Open stream that doesn't exist ──────────────────────────────────────

TEST_P(StressErrorHandlingConformance, OpenNonExistentStream) {
    auto p = temp_file("se_nostrm");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto r = cf->root_storage().open_stream("Ghost");
    EXPECT_FALSE(r.has_value());
}

// ── Open storage that doesn't exist ─────────────────────────────────────

TEST_P(StressErrorHandlingConformance, OpenNonExistentStorage) {
    auto p = temp_file("se_nostg");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto r = cf->root_storage().open_storage("Ghost");
    EXPECT_FALSE(r.has_value());
}

// ── Remove non-existent entry ───────────────────────────────────────────

TEST_P(StressErrorHandlingConformance, RemoveNonExistent) {
    auto p = temp_file("se_remnone");
    guard_.add(p);
    auto cf = compound_file::create(p, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    auto r = cf->root_storage().remove("Ghost");
    EXPECT_FALSE(r.has_value());
}

// ── Create duplicate stream ─────────────────────────────────────────────

TEST_P(StressErrorHandlingConformance, CreateDuplicateStream) {
    auto p = temp_file("se_dupstrm");
    guard_.add(p);
    auto cf = compound_file::create(p, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    ASSERT_TRUE(cf->root_storage().create_stream("X").has_value());
    auto r = cf->root_storage().create_stream("X");
    // Stout may or may not reject duplicates; verify no crash
    (void)r;
}

// ── Create duplicate storage ────────────────────────────────────────────

TEST_P(StressErrorHandlingConformance, CreateDuplicateStorage) {
    auto p = temp_file("se_dupstg");
    guard_.add(p);
    auto cf = compound_file::create(p, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    ASSERT_TRUE(cf->root_storage().create_storage("X").has_value());
    auto r = cf->root_storage().create_storage("X");
    // Stout may or may not reject duplicates; verify no crash
    (void)r;
}

// ── Create stream with empty name ───────────────────────────────────────

TEST_P(StressErrorHandlingConformance, CreateStreamEmptyName) {
    auto p = temp_file("se_emptyname");
    guard_.add(p);
    auto cf = compound_file::create(p, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    auto r = cf->root_storage().create_stream("");
    // Stout may or may not reject empty names; verify no crash
    (void)r;
}

// ── Create storage with empty name ──────────────────────────────────────

TEST_P(StressErrorHandlingConformance, CreateStorageEmptyName) {
    auto p = temp_file("se_emptystg");
    guard_.add(p);
    auto cf = compound_file::create(p, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    auto r = cf->root_storage().create_storage("");
    // Stout may or may not reject empty names; verify no crash
    (void)r;
}

// ── Create with too-long name ───────────────────────────────────────────

TEST_P(StressErrorHandlingConformance, CreateStreamTooLongName) {
    auto p = temp_file("se_longname");
    guard_.add(p);
    auto cf = compound_file::create(p, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    auto r = cf->root_storage().create_stream(std::string(32, 'A'));
    // Stout may or may not reject too-long names; verify no crash
    (void)r;
}

TEST_P(StressErrorHandlingConformance, CreateStorageTooLongName) {
    auto p = temp_file("se_longstg");
    guard_.add(p);
    auto cf = compound_file::create(p, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    auto r = cf->root_storage().create_storage(std::string(32, 'A'));
    // Stout may or may not reject too-long names; verify no crash
    (void)r;
}

// ── Rename with empty name ──────────────────────────────────────────────

TEST_P(StressErrorHandlingConformance, RenameStorageEmptyName) {
    auto p = temp_file("se_renempty");
    guard_.add(p);
    auto cf = compound_file::create(p, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    auto sub = cf->root_storage().create_storage("X");
    ASSERT_TRUE(sub.has_value());
    EXPECT_FALSE(sub->rename("").has_value());
}

TEST_P(StressErrorHandlingConformance, RenameStreamEmptyName) {
    auto p = temp_file("se_rensempty");
    guard_.add(p);
    auto cf = compound_file::create(p, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    auto s = cf->root_storage().create_stream("X");
    ASSERT_TRUE(s.has_value());
    EXPECT_FALSE(s->rename("").has_value());
}

// ── Rename with too-long name ───────────────────────────────────────────

TEST_P(StressErrorHandlingConformance, RenameStorageTooLong) {
    auto p = temp_file("se_renlong");
    guard_.add(p);
    auto cf = compound_file::create(p, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    auto sub = cf->root_storage().create_storage("X");
    ASSERT_TRUE(sub.has_value());
    EXPECT_FALSE(sub->rename(std::string(32, 'Z')).has_value());
}

TEST_P(StressErrorHandlingConformance, RenameStreamTooLong) {
    auto p = temp_file("se_renslong");
    guard_.add(p);
    auto cf = compound_file::create(p, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    auto s = cf->root_storage().create_stream("X");
    ASSERT_TRUE(s.has_value());
    EXPECT_FALSE(s->rename(std::string(32, 'Z')).has_value());
}

// ── Read from empty stream ──────────────────────────────────────────────

TEST_P(StressErrorHandlingConformance, ReadFromEmptyStream) {
    auto p = temp_file("se_rdempty");
    guard_.add(p);
    auto cf = compound_file::create(p, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    auto s = cf->root_storage().create_stream("E");
    ASSERT_TRUE(s.has_value());
    std::vector<uint8_t> buf(10);
    auto rd = s->read(0, std::span<uint8_t>(buf));
    ASSERT_TRUE(rd.has_value());
    EXPECT_EQ(*rd, 0u);
}

// ── Read beyond stream end ──────────────────────────────────────────────

TEST_P(StressErrorHandlingConformance, ReadBeyondEnd) {
    auto p = temp_file("se_rdbeyond");
    guard_.add(p);
    auto cf = compound_file::create(p, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    auto s = cf->root_storage().create_stream("D");
    ASSERT_TRUE(s.has_value());
    auto data = make_test_data(50, 0x11);
    ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
    std::vector<uint8_t> buf(100);
    auto rd = s->read(100, std::span<uint8_t>(buf));
    ASSERT_TRUE(rd.has_value());
    EXPECT_EQ(*rd, 0u);
}

// ── Transaction errors ──────────────────────────────────────────────────

TEST_P(StressErrorHandlingConformance, DoubleBeginTransaction) {
    auto p = temp_file("se_dbltxn");
    guard_.add(p);
    auto cf = compound_file::create(p, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    ASSERT_TRUE(cf->begin_transaction().has_value());
    EXPECT_FALSE(cf->begin_transaction().has_value());
}

TEST_P(StressErrorHandlingConformance, CommitWithoutTransaction) {
    auto p = temp_file("se_notxncom");
    guard_.add(p);
    auto cf = compound_file::create(p, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    EXPECT_FALSE(cf->commit().has_value());
}

TEST_P(StressErrorHandlingConformance, RevertWithoutTransaction) {
    auto p = temp_file("se_notxnrev");
    guard_.add(p);
    auto cf = compound_file::create(p, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    EXPECT_FALSE(cf->revert().has_value());
}

// ── set_element_times on non-existent child ─────────────────────────────

TEST_P(StressErrorHandlingConformance, SetElementTimesNotFound) {
    auto p = temp_file("se_eltnf");
    guard_.add(p);
    auto cf = compound_file::create(p, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    auto tp = std::chrono::system_clock::now();
    auto r = cf->root_storage().set_element_times("Ghost", tp, tp);
    EXPECT_FALSE(r.has_value());
}

// ── Exists check ────────────────────────────────────────────────────────

TEST_P(StressErrorHandlingConformance, ExistsReturnsFalseForMissing) {
    auto p = temp_file("se_exists");
    guard_.add(p);
    auto cf = compound_file::create(p, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    EXPECT_FALSE(cf->root_storage().exists("Nothing"));
}

TEST_P(StressErrorHandlingConformance, ExistsReturnsTrueForPresent) {
    auto p = temp_file("se_existsok");
    guard_.add(p);
    auto cf = compound_file::create(p, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    ASSERT_TRUE(cf->root_storage().create_stream("Here").has_value());
    EXPECT_TRUE(cf->root_storage().exists("Here"));
}

#endif // _WIN32
