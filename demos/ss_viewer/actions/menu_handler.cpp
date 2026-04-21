/**
 * @file menu_handler.cpp
 * @brief Menu action handler implementation.
 */
#include "ss_viewer/actions/menu_handler.h"

#include "ss_viewer/model/entry_info.h"

namespace ssv {

    void handle_menu(const std::string &label, viewer_state &st, tapiru::classic_app &app) {
        if (label == "Quit") {
            app.quit();
        } else if (label == "Expand All") {
            expand_all(st.root_entry, st.expanded);
            st.rebuild_flat_paths();
        } else if (label == "Collapse All") {
            st.expanded.clear();
            st.expanded.insert(tree_label(st.root_entry));
            st.rebuild_flat_paths();
            st.tree_cursor = 0;
            st.select_current();
        } else if (label == "Info Tab") {
            st.active_tab = 0;
            st.dirty = true;
        } else if (label == "Hex Tab") {
            st.active_tab = 1;
            st.dirty = true;
        } else if (label == "Properties Tab") {
            st.active_tab = 2;
            st.dirty = true;
        } else if (label == "Stats Tab") {
            st.active_tab = 3;
            st.dirty = true;
        } else if (label == "Toggle Theme") {
            st.use_dark_theme = !st.use_dark_theme;
            st.dirty = true;
        } else if (label == "Widen Tree") {
            if (st.tree_panel_width < viewer_state::tree_panel_max) {
                st.tree_panel_width += 2;
                st.dirty = true;
            }
        } else if (label == "Narrow Tree") {
            if (st.tree_panel_width > viewer_state::tree_panel_min) {
                st.tree_panel_width -= 2;
                st.dirty = true;
            }
        } else if (label == "Search") {
            st.show_search = true;
            st.search_query.clear();
            st.dirty = true;
        } else if (label == "Goto") {
            st.show_goto = true;
            st.goto_query.clear();
            st.goto_is_hex_offset = (st.active_tab == 1);
            st.dirty = true;
        } else if (label == "Toggle Bookmark") {
            st.toggle_bookmark();
        } else if (label == "Back") {
            st.nav_go_back();
        } else if (label == "Forward") {
            st.nav_go_forward();
        } else if (label == "Keybindings") {
            st.show_help = !st.show_help;
            st.dirty = true;
        }
    }

} // namespace ssv
