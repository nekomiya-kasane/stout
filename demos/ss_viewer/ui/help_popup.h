/**
 * @file help_popup.h
 * @brief Keybinding help popup overlay for ss_viewer.
 */
#pragma once

#include "tapiru/widgets/builders.h"
#include "tapiru/widgets/keybinding.h"
#include "tapiru/widgets/popup.h"

namespace ssv {

/// @brief Build the keybinding help popup.
[[nodiscard]] inline tapiru::popup_builder build_help_popup() {
    auto kb = tapiru::keybinding_builder()
                  .add("Up/Down", "Navigate tree")
                  .add("Enter/Right", "Expand storage")
                  .add("Left/BS", "Collapse / go to parent")
                  .add("Home/End", "Jump to first/last entry")
                  .add("Tab", "Next detail tab")
                  .add("Shift+Tab", "Previous detail tab")
                  .add("1/2/3/4", "Switch to Info/Hex/Props/Stats tab")
                  .add("PgUp/PgDn", "Scroll hex view")
                  .add("E", "Expand all nodes")
                  .add("C", "Collapse all nodes")
                  .add("t", "Toggle dark/light theme")
                  .add("+/-", "Widen/narrow tree panel")
                  .add("a", "Toggle ASCII column (hex tab)")
                  .add("Ctrl+C", "Copy to clipboard")
                  .add("Ctrl+F", "Search entries")
                  .add("Ctrl+G", "Goto entry/hex offset")
                  .add("Ctrl+B", "Toggle bookmark")
                  .add("Ctrl+E", "Export tree to JSON")
                  .add("Alt+Left", "Navigate back")
                  .add("Alt+Right", "Navigate forward")
                  .add("F1 / ?", "Toggle this help")
                  .add("Q / Esc", "Quit");

    auto popup = tapiru::popup_builder(std::move(kb));
    popup.title("Keyboard Shortcuts");
    popup.anchor(tapiru::popup_anchor::center);
    popup.dim_background(0.6f);
    return popup;
}

} // namespace ssv
