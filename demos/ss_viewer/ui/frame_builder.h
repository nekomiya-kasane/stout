/**
 * @file frame_builder.h
 * @brief Unified frame builder — single function used by interactive, dump, and dump-canvas modes.
 */
#pragma once

#include "ss_viewer/model/viewer_state.h"
#include "tapiru/widgets/builders.h"
#include "tapiru/widgets/classic_app.h"

namespace ssv {

    /// @brief Build the complete frame (menu bar + content + status bar) as a rows_builder.
    ///
    /// This is the single source of truth for the widget tree layout, called by
    /// interactive mode's set_content callback, --dump mode, and --dump-canvas mode.
    ///
    /// @param st         Application state (tree, selection, hex data, etc.)
    /// @param viewport_h Height available for the content area (excluding menu + status).
    /// @param theme      Visual theme for the classic_app shell.
    /// @return           A rows_builder representing the full frame.
    [[nodiscard]] tapiru::rows_builder build_frame(viewer_state &st, int viewport_h,
                                                   const tapiru::classic_app_theme &theme);

    /// @brief Build only the content area (tree + tabs), for use inside classic_app::set_content.
    ///
    /// @param st         Application state.
    /// @param viewport_h Height available for the content area.
    /// @return           A sized_box_builder filling the viewport.
    [[nodiscard]] tapiru::sized_box_builder build_content(viewer_state &st, int viewport_h);

    /// @brief Build the status bar for the current state.
    void build_status(const viewer_state &st, tapiru::status_bar_builder &sb);

} // namespace ssv
