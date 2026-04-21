/**
 * @file menus.h
 * @brief Menu bar definitions for ss_viewer.
 */
#pragma once

#include "tapiru/widgets/menu.h"
#include "tapiru/widgets/menu_bar.h"

#include <vector>

namespace ssv {

    /// @brief Build the application menu bar entries.
    [[nodiscard]] inline std::vector<tapiru::menu_bar_entry> build_menus() {
        return {
            {"File",
             {
                 tapiru::menu_item_builder("Export JSON").shortcut("Ctrl+E"),
                 tapiru::menu_item_builder::separator(),
                 tapiru::menu_item_builder("Quit").shortcut("q"),
             }},
            {"View",
             {
                 tapiru::menu_item_builder("Expand All").shortcut("E"),
                 tapiru::menu_item_builder("Collapse All").shortcut("C"),
                 tapiru::menu_item_builder::separator(),
                 tapiru::menu_item_builder("Info Tab").shortcut("1"),
                 tapiru::menu_item_builder("Hex Tab").shortcut("2"),
                 tapiru::menu_item_builder("Properties Tab").shortcut("3"),
                 tapiru::menu_item_builder("Stats Tab").shortcut("4"),
                 tapiru::menu_item_builder::separator(),
                 tapiru::menu_item_builder("Toggle Theme").shortcut("t"),
                 tapiru::menu_item_builder("Widen Tree").shortcut("+"),
                 tapiru::menu_item_builder("Narrow Tree").shortcut("-"),
             }},
            {"Navigate",
             {
                 tapiru::menu_item_builder("Search").shortcut("Ctrl+F"),
                 tapiru::menu_item_builder("Goto").shortcut("Ctrl+G"),
                 tapiru::menu_item_builder("Toggle Bookmark").shortcut("Ctrl+B"),
                 tapiru::menu_item_builder::separator(),
                 tapiru::menu_item_builder("Back").shortcut("Alt+Left"),
                 tapiru::menu_item_builder("Forward").shortcut("Alt+Right"),
             }},
            {"Help",
             {
                 tapiru::menu_item_builder("About"),
                 tapiru::menu_item_builder("Keybindings").shortcut("F1"),
             }},
        };
    }

} // namespace ssv
