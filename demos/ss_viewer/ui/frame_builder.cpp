/**
 * @file frame_builder.cpp
 * @brief Unified frame builder implementation.
 */
#include "ss_viewer/ui/frame_builder.h"

#include "ss_viewer/ui/help_popup.h"
#include "ss_viewer/ui/hex_tab.h"
#include "ss_viewer/ui/info_tab.h"
#include "ss_viewer/ui/menus.h"
#include "ss_viewer/ui/props_tab.h"
#include "ss_viewer/ui/stats_tab.h"
#include "ss_viewer/ui/theme.h"
#include "ss_viewer/util/format.h"
#include "tapiru/widgets/breadcrumb.h"
#include "tapiru/widgets/popup.h"
#include "tapiru/widgets/search_input.h"
#include "tapiru/widgets/tab.h"
#include "tapiru/widgets/toast.h"
#include "tapiru/widgets/tree_view.h"

#include <algorithm>
#include <format>
#include <string>

namespace ssv {

using namespace tapiru;

sized_box_builder build_content(viewer_state &st, int viewport_h) {
    auto vt = current_theme(st.use_dark_theme);

    // Build tree node
    auto tree_root = to_tree_node(st.root_entry);

    // Left pane: tree view in a panel
    auto tree = tree_view_builder()
                    .root(std::move(tree_root))
                    .expanded_set(&st.expanded)
                    .cursor(&st.tree_cursor)
                    .node_style(vt.tree_node)
                    .highlight_style(vt.tree_highlight)
                    .guide_style(vt.tree_guide);

    panel_builder tree_panel(std::move(tree));
    tree_panel.title("Storage Tree");
    tree_panel.border(border_style::rounded);
    tree_panel.border_style_override(vt.tree_border);

    // Right pane: tabs
    rows_builder right_pane;
    right_pane.gap(0);

    // Breadcrumb (with bookmark indicator)
    if (st.selected) {
        breadcrumb_builder bc;
        std::string path = st.selected->full_path;
        size_t pos = 0;
        while ((pos = path.find('/')) != std::string::npos) {
            bc.add_item(path.substr(0, pos));
            path.erase(0, pos + 1);
        }
        std::string leaf = path;
        if (st.is_bookmarked(st.selected->full_path)) leaf = "\xe2\x98\x85 " + leaf; // ★ prefix for bookmarked
        bc.add_item(leaf);
        bc.separator(" \xe2\x80\xba ");
        bc.item_style(vt.breadcrumb_item);
        bc.active_style(vt.breadcrumb_active);
        right_pane.add(std::move(bc));
    }

    // Tab content
    tab_builder tabs;

    // Info tab
    if (st.selected) {
        tabs.add_tab("Info", build_info_table(*st.selected));
    } else {
        tabs.add_tab("Info", text_builder("[dim]Select an entry in the tree[/]"));
    }

    // Hex tab
    if (st.selected && st.selected->type == stout::entry_type::stream && st.hex_reader.valid()) {
        tabs.add_tab("Hex", build_hex_tab(st, viewport_h));
    } else if (st.selected && st.selected->type == stout::entry_type::stream) {
        tabs.add_tab("Hex", text_builder("[dim]Empty stream[/]"));
    } else {
        tabs.add_tab("Hex", text_builder("[dim]Select a stream to view hex dump[/]"));
    }

    // Properties tab
    if (st.prop_set && !st.prop_set->sections.empty()) {
        rows_builder prop_rows;
        prop_rows.gap(0);
        for (auto &section : st.prop_set->sections) {
            prop_rows.add(text_builder(std::format("[bold bright_yellow]Section: {}[/]", format_guid(section.fmtid))));
            prop_rows.add(text_builder(std::format("[dim]Codepage: {}[/]", section.codepage)));
        }
        prop_rows.add(build_properties_table(*st.prop_set));
        tabs.add_tab("Properties", std::move(prop_rows));
    } else if (st.selected && is_property_stream(st.selected->name)) {
        tabs.add_tab("Properties", text_builder("[dim]Failed to parse property set[/]"));
    } else {
        tabs.add_tab("Properties", text_builder("[dim]Select a \\005 property stream[/]"));
    }

    // Stats tab
    tabs.add_tab("Stats", build_stats_tab(st.root_entry));

    tabs.active(&st.active_tab);
    tabs.tab_style(vt.tab);
    tabs.active_tab_style(vt.active_tab);
    tabs.content_border(border_style::rounded);
    tabs.content_border_style(vt.tab_content_border);

    right_pane.add(std::move(tabs), 1);

    // Wrap tree panel in a sized box with dynamic width
    sized_box_builder tree_box(std::move(tree_panel));
    tree_box.min_width(st.tree_panel_width);

    // Layout: left tree (fixed width) | right detail (flex)
    columns_builder layout;
    layout.gap(1);
    layout.add(std::move(tree_box), 0);
    layout.add(std::move(right_pane), 1);

    // Force the layout to fill the viewport height
    sized_box_builder sized(std::move(layout));
    sized.height(static_cast<uint32_t>(std::max(viewport_h, 1)));

    return sized;
}

void build_status(const viewer_state &st, status_bar_builder &sb) {
    std::string left = st.file_path.filename().string();
    if (st.use_win32)
        left += " [Win32]";
    else
        left += " [stout]";
    sb.left(left);

    sb.center(std::format("CFB {} | Sector {} | {}", st.version_str, st.sector_str, format_size(st.file_size)));

    if (st.selected)
        sb.right(std::format("{} | {}", entry_type_str(st.selected->type), format_size(st.selected->size)));
    else
        sb.right("No selection");
}

rows_builder build_frame(viewer_state &st, int viewport_h, const classic_app_theme &app_theme) {
    // Menu bar
    std::string bar = " ";
    auto menus = build_menus();
    for (auto &m : menus) {
        bar += "[rgb(" + std::to_string(app_theme.menu_bar_bg.fg.r) + "," + std::to_string(app_theme.menu_bar_bg.fg.g) +
               "," + std::to_string(app_theme.menu_bar_bg.fg.b) + ") on_rgb(" +
               std::to_string(app_theme.menu_bar_bg.bg.r) + "," + std::to_string(app_theme.menu_bar_bg.bg.g) + "," +
               std::to_string(app_theme.menu_bar_bg.bg.b) + ")] " + m.label + " [/]";
    }
    auto menu_bar = text_builder(bar);
    menu_bar.style_override(app_theme.menu_bar_bg);

    // Content area
    rows_builder content;
    content.gap(0);
    content.add(build_content(st, viewport_h));

    // Status bar
    status_bar_builder sb;
    build_status(st, sb);
    sb.style_override(app_theme.status_bar);

    // Assemble full frame
    rows_builder frame;
    frame.gap(0);
    frame.add(std::move(menu_bar));
    frame.add(std::move(content));
    frame.add(std::move(sb));

    // Search bar overlay
    if (st.show_search) {
        auto si = search_input_builder();
        si.query(&st.search_query);
        si.match_count(&st.search_match_count);
        si.current_match(&st.search_current_match);
        si.placeholder("Search entries... (Enter=next, Esc=close)");
        si.width(50);
        frame.add(std::move(si));
    }

    // Goto overlay
    if (st.show_goto) {
        std::string label = st.goto_is_hex_offset ? "Goto hex offset: " : "Goto entry path: ";
        frame.add(text_builder("[bold cyan]" + label + "[/][bold white]" + st.goto_query + "_[/]"));
    }

    // Help popup overlay
    if (st.show_help) {
        frame.add(build_help_popup());
    }

    // Toast overlay
    if (!st.toast_msg.empty() && std::chrono::steady_clock::now() < st.toast_until) {
        frame.add(text_builder("[bold green] \xe2\x9c\x93 " + st.toast_msg + " [/]"));
    } else {
        st.toast_msg.clear();
    }

    return frame;
}

} // namespace ssv
