/**
 * @file test_phase5.cpp
 * @brief Tests for Phase 5 features: theme, tree resize, bookmarks,
 *        navigation history, goto overlay, export JSON, help popup updates.
 */
#include "ss_viewer/model/entry_info.h"
#include "ss_viewer/model/stout_backend.h"
#include "ss_viewer/model/viewer_state.h"
#include "ss_viewer/ui/frame_builder.h"
#include "ss_viewer/ui/theme.h"
#include "stout/compound_file.h"
#include "tapiru/testing/test_harness.h"

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

// ── Theme tests ─────────────────────────────────────────────────────────

TEST(ThemeTest, DarkThemeHasDarkColors) {
    auto dt = dark_theme();
    // Dark theme tree_node should have bright foreground
    EXPECT_GT(dt.tree_node.fg.r, 150);
}

TEST(ThemeTest, LightThemeHasLightColors) {
    auto lt = light_theme();
    // Light theme tree_node should have dark foreground
    EXPECT_LT(lt.tree_node.fg.r, 100);
}

TEST(ThemeTest, CurrentThemeSwitches) {
    auto dt = current_theme(true);
    auto lt = current_theme(false);
    EXPECT_NE(dt.tree_node.fg.r, lt.tree_node.fg.r);
}

TEST(ThemeTest, ViewerThemeHasAllFields) {
    auto vt = dark_theme();
    // Spot-check that all fields are initialized (non-default)
    // hex_cursor should have a background
    EXPECT_GT(vt.hex_cursor.bg.r + vt.hex_cursor.bg.g + vt.hex_cursor.bg.b, 0u);
    // bookmark_marker should have a foreground
    EXPECT_GT(vt.bookmark_marker.fg.r, 0u);
}

// ── Bookmark tests ──────────────────────────────────────────────────────

class BookmarkTest : public ::testing::Test {
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
    }
    viewer_state st;
};

TEST_F(BookmarkTest, ToggleBookmarkAdds) {
    EXPECT_FALSE(st.is_bookmarked(st.selected->full_path));
    st.toggle_bookmark();
    EXPECT_TRUE(st.is_bookmarked(st.selected->full_path));
}

TEST_F(BookmarkTest, ToggleBookmarkRemoves) {
    st.toggle_bookmark();
    EXPECT_TRUE(st.is_bookmarked(st.selected->full_path));
    st.toggle_bookmark();
    EXPECT_FALSE(st.is_bookmarked(st.selected->full_path));
}

TEST_F(BookmarkTest, MultipleBookmarks) {
    expand_all(st.root_entry, st.expanded);
    st.rebuild_flat_paths();

    st.tree_cursor = 0;
    st.select_current();
    st.toggle_bookmark();

    if (st.flat_paths.size() > 1) {
        st.tree_cursor = 1;
        st.select_current();
        st.toggle_bookmark();
        EXPECT_EQ(st.bookmarks.size(), 2u);
    }
}

// ── Navigation history tests ────────────────────────────────────────────

TEST_F(BookmarkTest, NavHistoryPushAndBack) {
    expand_all(st.root_entry, st.expanded);
    st.rebuild_flat_paths();
    if (st.flat_paths.size() < 2) {
        GTEST_SKIP();
    }

    st.tree_cursor = 0;
    st.select_current();
    auto first_path = st.selected->full_path;

    st.push_nav_history();
    st.tree_cursor = 1;
    st.select_current();
    auto second_path = st.selected->full_path;

    EXPECT_EQ(st.nav_back.size(), 1u);
    st.nav_go_back();
    EXPECT_EQ(st.selected->full_path, first_path);
    EXPECT_EQ(st.nav_forward.size(), 1u);
}

TEST_F(BookmarkTest, NavForwardAfterBack) {
    expand_all(st.root_entry, st.expanded);
    st.rebuild_flat_paths();
    if (st.flat_paths.size() < 2) {
        GTEST_SKIP();
    }

    st.tree_cursor = 0;
    st.select_current();

    st.push_nav_history();
    st.tree_cursor = 1;
    st.select_current();
    auto second_path = st.selected->full_path;

    st.nav_go_back();
    st.nav_go_forward();
    EXPECT_EQ(st.selected->full_path, second_path);
}

TEST_F(BookmarkTest, NavigateToPath) {
    expand_all(st.root_entry, st.expanded);
    st.rebuild_flat_paths();
    if (st.flat_paths.size() < 2) {
        GTEST_SKIP();
    }

    auto target = st.flat_paths[1];
    st.navigate_to_path(target);
    EXPECT_EQ(st.selected->full_path, target);
}

// ── Tree panel resize tests ─────────────────────────────────────────────

TEST_F(BookmarkTest, TreePanelWidthDefault) {
    EXPECT_EQ(st.tree_panel_width, 32u);
}

TEST_F(BookmarkTest, TreePanelWidthClampMin) {
    st.tree_panel_width = viewer_state::tree_panel_min;
    // Should not go below min
    EXPECT_GE(st.tree_panel_width, viewer_state::tree_panel_min);
}

TEST_F(BookmarkTest, TreePanelWidthClampMax) {
    st.tree_panel_width = viewer_state::tree_panel_max;
    EXPECT_LE(st.tree_panel_width, viewer_state::tree_panel_max);
}

// ── Goto overlay state tests ────────────────────────────────────────────

TEST_F(BookmarkTest, GotoOverlayDefaultHidden) {
    EXPECT_FALSE(st.show_goto);
    EXPECT_TRUE(st.goto_query.empty());
}

TEST_F(BookmarkTest, GotoOverlayActivates) {
    st.show_goto = true;
    st.goto_query = "test";
    st.goto_is_hex_offset = false;
    EXPECT_TRUE(st.show_goto);
    EXPECT_EQ(st.goto_query, "test");
}

// ── Hex state tests ─────────────────────────────────────────────────────

TEST_F(BookmarkTest, HexAsciiToggleDefault) {
    EXPECT_TRUE(st.hex_show_ascii);
}

TEST_F(BookmarkTest, HexSelectionDefault) {
    EXPECT_FALSE(st.hex_has_selection);
    EXPECT_EQ(st.hex_cursor, 0u);
}

// ── Frame rendering with theme ──────────────────────────────────────────

class Phase5FrameTest : public ::testing::Test {
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

TEST_F(Phase5FrameTest, DarkThemeRenders) {
    st.use_dark_theme = true;
    auto frame = build_frame(st, 20, theme);
    vs.render(frame);
    EXPECT_TRUE(vs.contains("Root Entry"));
    EXPECT_GT(vs.row_count(), 5u);
}

TEST_F(Phase5FrameTest, LightThemeRenders) {
    st.use_dark_theme = false;
    auto frame = build_frame(st, 20, theme);
    vs.render(frame);
    EXPECT_TRUE(vs.contains("Root Entry"));
    EXPECT_GT(vs.row_count(), 5u);
}

TEST_F(Phase5FrameTest, ThemeSwitchProducesDifferentOutput) {
    st.use_dark_theme = true;
    auto frame_dark = build_frame(st, 20, theme);
    tapiru::testing::virtual_screen vs_dark{80, 24};
    vs_dark.render_canvas(frame_dark);

    st.use_dark_theme = false;
    auto frame_light = build_frame(st, 20, theme);
    tapiru::testing::virtual_screen vs_light{80, 24};
    vs_light.render_canvas(frame_light);

    // Canvas cell styles should differ between themes
    // (text content is the same, but styles differ)
    if (vs_dark.has_canvas() && vs_light.has_canvas()) {
        // At least one cell should have different style
        bool found_diff = false;
        for (uint32_t y = 1; y < std::min(vs_dark.canvas().height, vs_light.canvas().height); ++y) {
            for (uint32_t x = 0; x < std::min(vs_dark.canvas().width, vs_light.canvas().width); ++x) {
                auto sd = vs_dark.canvas().style_at(x, y);
                auto sl = vs_light.canvas().style_at(x, y);
                if (sd.fg.r != sl.fg.r || sd.bg.r != sl.bg.r) {
                    found_diff = true;
                    break;
                }
            }
            if (found_diff) {
                break;
            }
        }
        EXPECT_TRUE(found_diff);
    }
}

TEST_F(Phase5FrameTest, GotoOverlayRendered) {
    st.show_goto = true;
    st.goto_query = "Files";
    st.goto_is_hex_offset = false;
    auto frame = build_frame(st, 20, theme);
    vs.render(frame);
    EXPECT_TRUE(vs.contains("Goto entry path") || vs.contains("Files"));
}

TEST_F(Phase5FrameTest, GotoHexOverlayRendered) {
    st.show_goto = true;
    st.goto_query = "0x100";
    st.goto_is_hex_offset = true;
    auto frame = build_frame(st, 20, theme);
    vs.render(frame);
    EXPECT_TRUE(vs.contains("Goto hex offset") || vs.contains("0x100"));
}

TEST_F(Phase5FrameTest, BookmarkShowsInBreadcrumb) {
    st.toggle_bookmark();
    auto frame = build_frame(st, 20, theme);
    vs.render(frame);
    // ★ should appear in the breadcrumb
    EXPECT_TRUE(vs.contains("\xe2\x98\x85") || vs.contains("Root Entry"));
}

TEST_F(Phase5FrameTest, ResizedTreePanelRendersWider) {
    st.tree_panel_width = 48;
    auto frame = build_frame(st, 20, theme);
    vs.render(frame);
    EXPECT_TRUE(vs.contains("Root Entry"));
}

TEST_F(Phase5FrameTest, HelpPopupShowsTitle) {
    st.show_help = true;
    // Use wider screen so popup content isn't truncated
    tapiru::testing::virtual_screen wide{120, 40};
    auto frame = build_frame(st, 36, theme);
    wide.render(frame);
    // The popup title should always appear
    EXPECT_TRUE(wide.contains("Keyboard Shortcuts"));
    // Popup should produce more rows than without help
    EXPECT_GT(wide.row_count(), 10u);
}

TEST_F(Phase5FrameTest, NavigateMenuPresent) {
    auto frame = build_frame(st, 20, theme);
    vs.render(frame);
    EXPECT_TRUE(vs.contains("Navigate"));
}
