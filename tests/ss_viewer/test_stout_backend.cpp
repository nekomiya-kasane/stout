/**
 * @file test_stout_backend.cpp
 * @brief Unit tests for stout backend: build_stout_tree, build_stout_tree_shallow,
 *        load_stout_children, read_stout_stream.
 */
#include "ss_viewer/model/entry_info.h"
#include "ss_viewer/model/stout_backend.h"
#include "stout/compound_file.h"

#include <filesystem>
#include <gtest/gtest.h>

using namespace ssv;

static std::filesystem::path test_cfb_path() {
    // testdata/stout_demo.cfb relative to the build working directory
    auto p = std::filesystem::path("testdata/stout_demo.cfb");
    if (!std::filesystem::exists(p)) {
        // Try from the repo root
        p = std::filesystem::path(STOUT_TESTDATA_DIR) / "stout_demo.cfb";
    }
    return p;
}

class StoutBackendTest : public ::testing::Test {
  protected:
    void SetUp() override {
        auto path = test_cfb_path();
        ASSERT_TRUE(std::filesystem::exists(path)) << "Test file not found: " << path;
        auto result = stout::compound_file::open(path, stout::open_mode::read);
        ASSERT_TRUE(result.has_value()) << "Failed to open test CFB file";
        cf.emplace(std::move(*result));
    }

    std::optional<stout::compound_file> cf;

    entry_info build_tree() {
        auto root_stg = cf->root_storage();
        return build_stout_tree(root_stg, "");
    }

    entry_info build_tree_shallow() {
        auto root_stg = cf->root_storage();
        return build_stout_tree_shallow(root_stg, "");
    }
};

// ── build_stout_tree ────────────────────────────────────────────────────

TEST_F(StoutBackendTest, BuildTreeHasRoot) {
    auto root = build_tree();
    EXPECT_EQ(root.name, "Root Entry");
    EXPECT_EQ(root.type, stout::entry_type::root);
    EXPECT_FALSE(root.children.empty());
    EXPECT_TRUE(root.children_loaded);
}

TEST_F(StoutBackendTest, BuildTreeHasStreams) {
    auto root = build_tree();
    bool has_stream = false;
    for (auto &c : root.children) {
        if (c.type == stout::entry_type::stream) {
            has_stream = true;
            EXPECT_FALSE(c.name.empty());
            EXPECT_FALSE(c.full_path.empty());
            break;
        }
    }
    EXPECT_TRUE(has_stream);
}

TEST_F(StoutBackendTest, BuildTreeFullPathsCorrect) {
    auto root = build_tree();
    EXPECT_EQ(root.full_path, "Root Entry");
    for (auto &c : root.children) {
        EXPECT_TRUE(c.full_path.starts_with("Root Entry/"));
        EXPECT_EQ(c.full_path, "Root Entry/" + c.name);
    }
}

// ── build_stout_tree_shallow ────────────────────────────────────────────

TEST_F(StoutBackendTest, ShallowTreeRootLoaded) {
    auto root = build_tree_shallow();
    EXPECT_EQ(root.name, "Root Entry");
    EXPECT_TRUE(root.children_loaded);
    EXPECT_FALSE(root.children.empty());
}

TEST_F(StoutBackendTest, ShallowTreeSubStoragesNotLoaded) {
    auto root = build_tree_shallow();
    for (auto &c : root.children) {
        if (c.type == stout::entry_type::storage) {
            EXPECT_FALSE(c.children_loaded);
            EXPECT_TRUE(c.children.empty());
        }
    }
}

// ── load_stout_children ─────────────────────────────────────────────────

TEST_F(StoutBackendTest, LoadChildrenPopulatesStorage) {
    auto root = build_tree_shallow();
    for (auto &c : root.children) {
        if (c.type == stout::entry_type::storage && !c.children_loaded) {
            load_stout_children(*cf, c);
            EXPECT_TRUE(c.children_loaded);
            break;
        }
    }
}

// ── read_stout_stream ───────────────────────────────────────────────────

TEST_F(StoutBackendTest, ReadStreamReturnsData) {
    auto root = build_tree();
    for (auto &c : root.children) {
        if (c.type == stout::entry_type::stream && c.size > 0) {
            auto data = read_stout_stream(*cf, c);
            EXPECT_FALSE(data.empty());
            EXPECT_LE(data.size(), 65536u);
            break;
        }
    }
}

TEST_F(StoutBackendTest, OpenStoutReaderValid) {
    auto root = build_tree();
    for (auto &c : root.children) {
        if (c.type == stout::entry_type::stream && c.size > 0) {
            auto reader = open_stout_reader(*cf, c);
            EXPECT_TRUE(reader.valid());
            EXPECT_GT(reader.total_size(), 0u);
            EXPECT_GT(reader.total_lines(), 0u);
            break;
        }
    }
}
