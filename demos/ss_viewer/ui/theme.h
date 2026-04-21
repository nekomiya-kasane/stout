/**
 * @file theme.h
 * @brief Centralized style constants for ss_viewer UI — dark and light themes.
 */
#pragma once

#include "tapiru/core/style.h"

namespace ssv {

    using tapiru::attr;
    using tapiru::color;
    using tapiru::style;

    /// @brief A complete set of styles for the viewer UI.
    struct viewer_theme {
        style tree_border;
        style tab;
        style active_tab;
        style tree_node;
        style tree_highlight;
        style tree_guide;
        style hex;
        style hex_cursor;
        style hex_selection;
        style info_border;
        style info_header;
        style props_border;
        style props_header;
        style tab_content_border;
        style breadcrumb_item;
        style breadcrumb_active;
        style bookmark_marker;
    };

    /// @brief Dark theme (default).
    [[nodiscard]] inline viewer_theme dark_theme() {
        return {
            .tree_border = {color::from_rgb(80, 80, 120)},
            .tab = {color::from_rgb(140, 140, 160), color::from_rgb(30, 30, 40)},
            .active_tab = {color::from_rgb(255, 255, 255), color::from_rgb(50, 50, 80), attr::bold},
            .tree_node = {color::from_rgb(200, 200, 220)},
            .tree_highlight = {color::from_rgb(255, 255, 255), color::from_rgb(50, 60, 100), attr::bold},
            .tree_guide = {color::from_rgb(60, 60, 80)},
            .hex = {color::from_rgb(160, 200, 160)},
            .hex_cursor = {color::from_rgb(255, 255, 255), color::from_rgb(80, 80, 40), attr::bold},
            .hex_selection = {color::from_rgb(255, 255, 200), color::from_rgb(60, 60, 100)},
            .info_border = {color::from_rgb(80, 80, 120)},
            .info_header = {color::from_rgb(150, 200, 255), {}, attr::bold},
            .props_border = {color::from_rgb(80, 120, 80)},
            .props_header = {color::from_rgb(150, 255, 150), {}, attr::bold},
            .tab_content_border = {color::from_rgb(80, 80, 120)},
            .breadcrumb_item = {color::from_rgb(120, 120, 160)},
            .breadcrumb_active = {color::from_rgb(200, 200, 255), {}, attr::bold},
            .bookmark_marker = {color::from_rgb(255, 200, 80), {}, attr::bold},
        };
    }

    /// @brief Light theme.
    [[nodiscard]] inline viewer_theme light_theme() {
        return {
            .tree_border = {color::from_rgb(120, 120, 180)},
            .tab = {color::from_rgb(80, 80, 100), color::from_rgb(220, 220, 230)},
            .active_tab = {color::from_rgb(20, 20, 40), color::from_rgb(200, 210, 240), attr::bold},
            .tree_node = {color::from_rgb(40, 40, 60)},
            .tree_highlight = {color::from_rgb(0, 0, 0), color::from_rgb(180, 200, 240), attr::bold},
            .tree_guide = {color::from_rgb(160, 160, 180)},
            .hex = {color::from_rgb(40, 100, 40)},
            .hex_cursor = {color::from_rgb(0, 0, 0), color::from_rgb(255, 255, 180), attr::bold},
            .hex_selection = {color::from_rgb(0, 0, 40), color::from_rgb(200, 220, 255)},
            .info_border = {color::from_rgb(120, 120, 180)},
            .info_header = {color::from_rgb(30, 80, 180), {}, attr::bold},
            .props_border = {color::from_rgb(80, 140, 80)},
            .props_header = {color::from_rgb(30, 120, 30), {}, attr::bold},
            .tab_content_border = {color::from_rgb(120, 120, 180)},
            .breadcrumb_item = {color::from_rgb(80, 80, 120)},
            .breadcrumb_active = {color::from_rgb(20, 20, 80), {}, attr::bold},
            .bookmark_marker = {color::from_rgb(200, 140, 0), {}, attr::bold},
        };
    }

    /// @brief Get the current theme based on dark/light preference.
    [[nodiscard]] inline viewer_theme current_theme(bool use_dark) {
        return use_dark ? dark_theme() : light_theme();
    }

    // Backward-compatible namespace for code that uses theme::tree_border etc.
    namespace theme {
        inline const style tree_border{color::from_rgb(80, 80, 120)};
        inline const style tab{color::from_rgb(140, 140, 160), color::from_rgb(30, 30, 40)};
        inline const style active_tab{color::from_rgb(255, 255, 255), color::from_rgb(50, 50, 80), attr::bold};
        inline const style tree_node{color::from_rgb(200, 200, 220)};
        inline const style tree_highlight{color::from_rgb(255, 255, 255), color::from_rgb(50, 60, 100), attr::bold};
        inline const style tree_guide{color::from_rgb(60, 60, 80)};
        inline const style hex{color::from_rgb(160, 200, 160)};
        inline const style info_border{color::from_rgb(80, 80, 120)};
        inline const style info_header{color::from_rgb(150, 200, 255), {}, attr::bold};
        inline const style props_border{color::from_rgb(80, 120, 80)};
        inline const style props_header{color::from_rgb(150, 255, 150), {}, attr::bold};
        inline const style tab_content_border{color::from_rgb(80, 80, 120)};
        inline const style breadcrumb_item{color::from_rgb(120, 120, 160)};
        inline const style breadcrumb_active{color::from_rgb(200, 200, 255), {}, attr::bold};
    } // namespace theme

} // namespace ssv
