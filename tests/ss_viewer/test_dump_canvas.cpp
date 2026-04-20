/**
 * @file test_dump_canvas.cpp
 * @brief Regression test: verify --dump-canvas output structure.
 *
 * Instead of an exact golden-file match (timestamps change), we verify
 * structural invariants: canvas dimensions, presence of key UI elements,
 * row count, and absence of blank/corrupt output.
 */
#include "ss_viewer/model/entry_info.h"
#include "ss_viewer/model/stout_backend.h"
#include "ss_viewer/model/viewer_state.h"
#include "ss_viewer/ui/frame_builder.h"
#include "stout/compound_file.h"
#include "tapiru/testing/test_harness.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <sstream>

using namespace ssv;

static std::filesystem::path test_cfb_path() {
    auto p = std::filesystem::path("testdata/stout_demo.cfb");
    if (!std::filesystem::exists(p)) p = std::filesystem::path(STOUT_TESTDATA_DIR) / "stout_demo.cfb";
    return p;
}

class DumpCanvasTest : public ::testing::Test {
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
        st.file_path = path;

        // Render the frame via render_canvas for cell-level inspection
        auto frame = build_frame(st, 25, theme);
        vs.render_canvas(frame);
    }

    viewer_state st;
    tapiru::testing::virtual_screen vs{120, 29};
    tapiru::classic_app_theme theme = tapiru::classic_app_theme::dark();
};

// ── Canvas dimensions ───────────────────────────────────────────────────

TEST_F(DumpCanvasTest, CanvasHasReasonableDimensions) {
    EXPECT_TRUE(vs.has_canvas());
    EXPECT_EQ(vs.canvas().width, 120u);
    EXPECT_GT(vs.canvas().height, 10u);
}

TEST_F(DumpCanvasTest, RowCountMatchesCanvasHeight) {
    EXPECT_EQ(vs.row_count(), vs.canvas().height);
}

// ── Menu bar (row 0) ────────────────────────────────────────────────────

TEST_F(DumpCanvasTest, MenuBarPresent) {
    EXPECT_TRUE(vs.contains("File"));
    EXPECT_TRUE(vs.contains("View"));
    EXPECT_TRUE(vs.contains("Help"));
}

TEST_F(DumpCanvasTest, MenuBarIsFirstRow) {
    auto row = vs.find_row("File");
    EXPECT_EQ(row, 0);
}

// ── Status bar (last row) ───────────────────────────────────────────────

TEST_F(DumpCanvasTest, StatusBarPresent) {
    EXPECT_TRUE(vs.contains("stout_demo.cfb"));
    EXPECT_TRUE(vs.contains("CFB v4"));
    EXPECT_TRUE(vs.contains("4096 B"));
}

TEST_F(DumpCanvasTest, StatusBarIsLastRow) {
    int last = static_cast<int>(vs.row_count()) - 1;
    ASSERT_GE(last, 0);
    auto &row = vs.row_text(last);
    EXPECT_TRUE(row.find("stout_demo.cfb") != std::string::npos || row.find("CFB v4") != std::string::npos);
}

// ── Tree panel ──────────────────────────────────────────────────────────

TEST_F(DumpCanvasTest, TreePanelShowsRoot) {
    EXPECT_TRUE(vs.contains("Root Entry"));
}

TEST_F(DumpCanvasTest, TreePanelHasBorder) {
    EXPECT_TRUE(vs.contains("Storage Tree") || vs.contains("\xe2\x94\x82"));
}

// ── Tab bar ─────────────────────────────────────────────────────────────

TEST_F(DumpCanvasTest, AllFourTabsPresent) {
    EXPECT_TRUE(vs.contains("Info"));
    EXPECT_TRUE(vs.contains("Hex"));
    EXPECT_TRUE(vs.contains("Properties"));
    EXPECT_TRUE(vs.contains("Stats"));
}

// ── Info tab content (default) ──────────────────────────────────────────

TEST_F(DumpCanvasTest, InfoTabShowsPropertyTable) {
    EXPECT_TRUE(vs.contains("Name"));
    EXPECT_TRUE(vs.contains("Type"));
    EXPECT_TRUE(vs.contains("Root Storage") || vs.contains("Root Entry"));
}

TEST_F(DumpCanvasTest, InfoTabShowsPath) {
    EXPECT_TRUE(vs.contains("Path"));
}

// ── No blank output ─────────────────────────────────────────────────────

TEST_F(DumpCanvasTest, NoCompletelyBlankRows) {
    // Every row should have at least some non-space content
    // (except possibly padding rows in the detail pane)
    int blank_count = 0;
    for (uint32_t i = 0; i < vs.row_count(); ++i) {
        auto &row = vs.row_text(i);
        bool all_space = true;
        for (char c : row) {
            if (c != ' ' && c != '\0') {
                all_space = false;
                break;
            }
        }
        if (all_space) ++blank_count;
    }
    // Allow some blank rows (padding) but not all
    EXPECT_LT(blank_count, static_cast<int>(vs.row_count()) / 2);
}
