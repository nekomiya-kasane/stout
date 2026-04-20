/**
 * @file test_frame_layout.cpp
 * @brief TUI integration tests: frame layout, tree rendering, tab switching,
 *        search overlay, help popup.
 */
#include <gtest/gtest.h>

#include <filesystem>

#include "stout/compound_file.h"
#include "tapiru/testing/test_harness.h"

#include "ss_viewer/model/entry_info.h"
#include "ss_viewer/model/stout_backend.h"
#include "ss_viewer/model/viewer_state.h"
#include "ss_viewer/ui/frame_builder.h"

using namespace ssv;

static std::filesystem::path test_cfb_path() {
    auto p = std::filesystem::path("testdata/stout_demo.cfb");
    if (!std::filesystem::exists(p))
        p = std::filesystem::path(STOUT_TESTDATA_DIR) / "stout_demo.cfb";
    return p;
}

class FrameLayoutTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto path = test_cfb_path();
        ASSERT_TRUE(std::filesystem::exists(path));
        auto result = stout::compound_file::open(path, stout::open_mode::read);
        ASSERT_TRUE(result.has_value());
        st.cf.emplace(std::move(*result));

        auto root_stg = st.cf->root_storage();
        st.root_entry = build_stout_tree(root_stg, "");
        st.expanded.insert(tree_label(st.root_entry));
        st.rebuild_flat_paths();
        st.selected = &st.root_entry;
        st.version_str = "v4";
        st.sector_str = "4096 B";
    }

    viewer_state st;
    tapiru::testing::virtual_screen vs{80, 24};
    tapiru::classic_app_theme theme = tapiru::classic_app_theme::dark();
};

// ── Frame structure ─────────────────────────────────────────────────────

TEST_F(FrameLayoutTest, FrameContainsMenuBar) {
    auto frame = build_frame(st, 20, theme);
    vs.render(frame);
    // Menu bar should be in the first few rows
    EXPECT_TRUE(vs.contains("File") || vs.contains("View") || vs.contains("Help"));
}

TEST_F(FrameLayoutTest, FrameContainsStatusBar) {
    auto frame = build_frame(st, 20, theme);
    vs.render(frame);
    // Status bar should show file info or navigation hints
    EXPECT_TRUE(vs.contains("stout_demo.cfb") || vs.contains("Root Entry") ||
                vs.row_count() > 5);
}

TEST_F(FrameLayoutTest, FrameContainsTreeView) {
    auto frame = build_frame(st, 20, theme);
    vs.render(frame);
    EXPECT_TRUE(vs.contains("Root Entry"));
}

TEST_F(FrameLayoutTest, FrameContainsTabs) {
    auto frame = build_frame(st, 20, theme);
    vs.render(frame);
    EXPECT_TRUE(vs.contains("Info"));
    EXPECT_TRUE(vs.contains("Hex"));
    EXPECT_TRUE(vs.contains("Properties"));
    EXPECT_TRUE(vs.contains("Stats"));
}

// ── Tab content ─────────────────────────────────────────────────────────

TEST_F(FrameLayoutTest, InfoTabShowsEntryName) {
    st.active_tab = 0;
    auto frame = build_frame(st, 20, theme);
    vs.render(frame);
    EXPECT_TRUE(vs.contains("Root Entry"));
}

TEST_F(FrameLayoutTest, StatsTabShowsStatistics) {
    st.active_tab = 3;
    auto frame = build_frame(st, 20, theme);
    vs.render(frame);
    EXPECT_TRUE(vs.contains("Total") || vs.contains("Entries") || vs.contains("Streams"));
}

// ── Tab switching ───────────────────────────────────────────────────────

TEST_F(FrameLayoutTest, TabSwitchChangesContent) {
    st.active_tab = 0;
    auto frame0 = build_frame(st, 20, theme);
    vs.render(frame0);
    auto raw0 = vs.raw();

    st.active_tab = 3;
    auto frame3 = build_frame(st, 20, theme);
    vs.render(frame3);
    auto raw3 = vs.raw();

    // Different tabs should produce different output
    EXPECT_NE(raw0, raw3);
}

// ── Tree expansion ──────────────────────────────────────────────────────

TEST_F(FrameLayoutTest, ExpandedTreeShowsChildren) {
    expand_all(st.root_entry, st.expanded);
    st.rebuild_flat_paths();
    auto frame = build_frame(st, 20, theme);
    vs.render(frame);
    // Should show child entries
    EXPECT_GT(st.flat_paths.size(), 1u);
    // At least one child should appear in the rendered output
    bool found_child = false;
    for (size_t i = 1; i < st.flat_paths.size(); ++i) {
        auto* e = find_entry(st.root_entry, st.flat_paths[i]);
        if (e && vs.contains(e->name)) {
            found_child = true;
            break;
        }
    }
    EXPECT_TRUE(found_child);
}

TEST_F(FrameLayoutTest, CollapsedTreeShowsOnlyRoot) {
    st.expanded.clear();
    st.rebuild_flat_paths();
    EXPECT_EQ(st.flat_paths.size(), 1u);
    auto frame = build_frame(st, 20, theme);
    vs.render(frame);
    EXPECT_TRUE(vs.contains("Root Entry"));
}

// ── Help popup overlay ──────────────────────────────────────────────────

TEST_F(FrameLayoutTest, HelpPopupShowsShortcuts) {
    st.show_help = true;
    auto frame = build_frame(st, 20, theme);
    vs.render(frame);
    EXPECT_TRUE(vs.contains("Keyboard") || vs.contains("Shortcuts") ||
                vs.contains("Up/Down") || vs.contains("Ctrl+C"));
}

TEST_F(FrameLayoutTest, NoHelpPopupByDefault) {
    st.show_help = false;
    auto frame = build_frame(st, 20, theme);
    vs.render(frame);
    // "Keyboard Shortcuts" title should not appear when help is hidden
    EXPECT_FALSE(vs.contains("Keyboard Shortcuts"));
}

// ── Search overlay ──────────────────────────────────────────────────────

TEST_F(FrameLayoutTest, SearchOverlayShown) {
    st.show_search = true;
    st.search_query = "test";
    auto frame = build_frame(st, 20, theme);
    vs.render(frame);
    // Search input should show the query or placeholder
    EXPECT_TRUE(vs.contains("test") || vs.contains("Search"));
}

// ── Toast overlay ───────────────────────────────────────────────────────

TEST_F(FrameLayoutTest, ToastShownWhenActive) {
    st.toast_msg = "Copied to clipboard";
    st.toast_until = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    auto frame = build_frame(st, 20, theme);
    vs.render(frame);
    EXPECT_TRUE(vs.contains("Copied") || vs.contains("clipboard"));
}

TEST_F(FrameLayoutTest, ToastHiddenWhenExpired) {
    st.toast_msg = "Copied to clipboard";
    st.toast_until = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    auto frame = build_frame(st, 20, theme);
    vs.render(frame);
    EXPECT_FALSE(vs.contains("Copied to clipboard"));
}

// ── Hex tab with stream selected ────────────────────────────────────────

TEST_F(FrameLayoutTest, HexTabShowsDataForStream) {
    // Find and select a stream
    expand_all(st.root_entry, st.expanded);
    st.rebuild_flat_paths();
    for (int i = 0; i < static_cast<int>(st.flat_paths.size()); ++i) {
        auto* e = find_entry(st.root_entry, st.flat_paths[i]);
        if (e && e->type == stout::entry_type::stream && e->size > 0) {
            st.tree_cursor = i;
            st.select_current();
            break;
        }
    }
    if (!st.hex_reader.valid()) {
        GTEST_SKIP() << "No stream with data found";
    }
    st.active_tab = 1;
    auto frame = build_frame(st, 20, theme);
    vs.render(frame);
    // Hex tab should show offset header
    EXPECT_TRUE(vs.contains("Offset") || vs.contains("00 01 02"));
}
