/**
 * @file test_viewer_state.cpp
 * @brief Unit tests for viewer_state: select_current, rebuild_flat_paths,
 *        expand/collapse, load_hex_data.
 */
#include "ss_viewer/model/entry_info.h"
#include "ss_viewer/model/stout_backend.h"
#include "ss_viewer/model/viewer_state.h"
#include "stout/compound_file.h"

#include <filesystem>
#include <gtest/gtest.h>

using namespace ssv;

static std::filesystem::path test_cfb_path() {
    auto p = std::filesystem::path("testdata/stout_demo.cfb");
    if (!std::filesystem::exists(p)) {
        p = std::filesystem::path(STOUT_TESTDATA_DIR) / "stout_demo.cfb";
    }
    return p;
}

class ViewerStateTest : public ::testing::Test {
  protected:
    void SetUp() override {
        auto path = test_cfb_path();
        ASSERT_TRUE(std::filesystem::exists(path)) << "Test file not found: " << path;

        auto result = stout::compound_file::open(path, stout::open_mode::read);
        ASSERT_TRUE(result.has_value());
        st.cf.emplace(std::move(*result));

        auto root_stg = st.cf->root_storage();
        st.root_entry = build_stout_tree(root_stg, "");
        st.expanded.insert(tree_label(st.root_entry));
        st.rebuild_flat_paths();
        st.selected = &st.root_entry;
    }

    viewer_state st;
};

// ── rebuild_flat_paths ──────────────────────────────────────────────────

TEST_F(ViewerStateTest, FlatPathsNotEmpty) {
    EXPECT_FALSE(st.flat_paths.empty());
    EXPECT_EQ(st.flat_paths[0], "Root Entry");
}

TEST_F(ViewerStateTest, FlatPathsGrowOnExpand) {
    auto initial_size = st.flat_paths.size();
    // Expand all
    expand_all(st.root_entry, st.expanded);
    st.rebuild_flat_paths();
    EXPECT_GE(st.flat_paths.size(), initial_size);
}

// ── select_current ──────────────────────────────────────────────────────

TEST_F(ViewerStateTest, SelectCurrentUpdatesSelected) {
    st.tree_cursor = 0;
    st.select_current();
    ASSERT_NE(st.selected, nullptr);
    EXPECT_EQ(st.selected->full_path, st.flat_paths[0]);
}

TEST_F(ViewerStateTest, SelectCurrentSetsStream) {
    // Find a stream in flat_paths
    for (int i = 0; i < static_cast<int>(st.flat_paths.size()); ++i) {
        auto *e = find_entry(st.root_entry, st.flat_paths[i]);
        if (e && e->type == stout::entry_type::stream) {
            st.tree_cursor = i;
            st.select_current();
            ASSERT_NE(st.selected, nullptr);
            EXPECT_EQ(st.selected->type, stout::entry_type::stream);
            return;
        }
    }
    GTEST_SKIP() << "No stream found in test file";
}

// ── load_hex_data ───────────────────────────────────────────────────────

TEST_F(ViewerStateTest, LoadHexDataForStream) {
    for (int i = 0; i < static_cast<int>(st.flat_paths.size()); ++i) {
        auto *e = find_entry(st.root_entry, st.flat_paths[i]);
        if (e && e->type == stout::entry_type::stream && e->size > 0) {
            st.tree_cursor = i;
            st.select_current();
            EXPECT_TRUE(st.hex_reader.valid());
            EXPECT_GT(st.hex_reader.total_size(), 0u);
            EXPECT_FALSE(st.hex_data.empty());
            return;
        }
    }
    GTEST_SKIP() << "No non-empty stream found";
}

TEST_F(ViewerStateTest, LoadHexDataClearsForStorage) {
    // First select a stream to populate hex data
    for (int i = 0; i < static_cast<int>(st.flat_paths.size()); ++i) {
        auto *e = find_entry(st.root_entry, st.flat_paths[i]);
        if (e && e->type == stout::entry_type::stream && e->size > 0) {
            st.tree_cursor = i;
            st.select_current();
            break;
        }
    }
    // Now select root (storage) — hex data should clear
    st.tree_cursor = 0;
    st.select_current();
    EXPECT_FALSE(st.hex_reader.valid());
    EXPECT_TRUE(st.hex_data.empty());
}

TEST_F(ViewerStateTest, HexCacheAvoidsDuplicateLoad) {
    for (int i = 0; i < static_cast<int>(st.flat_paths.size()); ++i) {
        auto *e = find_entry(st.root_entry, st.flat_paths[i]);
        if (e && e->type == stout::entry_type::stream && e->size > 0) {
            st.tree_cursor = i;
            st.select_current();
            auto cached_path = st.hex_cached_path;
            // Select same entry again — should hit cache
            st.select_current();
            EXPECT_EQ(st.hex_cached_path, cached_path);
            return;
        }
    }
}

// ── dirty flag ──────────────────────────────────────────────────────────

TEST_F(ViewerStateTest, DirtySetOnRebuild) {
    st.dirty = false;
    st.rebuild_flat_paths();
    EXPECT_TRUE(st.dirty);
}

TEST_F(ViewerStateTest, DirtySetOnSelect) {
    st.dirty = false;
    st.tree_cursor = 0;
    st.select_current();
    EXPECT_TRUE(st.dirty);
}

// ── expand / collapse ───────────────────────────────────────────────────

TEST_F(ViewerStateTest, CollapseReducesFlatPaths) {
    expand_all(st.root_entry, st.expanded);
    st.rebuild_flat_paths();
    auto expanded_size = st.flat_paths.size();

    st.expanded.clear();
    st.rebuild_flat_paths();
    EXPECT_LT(st.flat_paths.size(), expanded_size);
    EXPECT_EQ(st.flat_paths.size(), 1u); // only root
}
