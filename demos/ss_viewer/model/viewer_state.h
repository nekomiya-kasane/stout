/**
 * @file viewer_state.h
 * @brief Application state for ss_viewer — selection, caches, backends.
 */
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "ss_viewer/model/entry_info.h"
#include "ss_viewer/model/paged_reader.h"
#include "ss_viewer/model/stout_backend.h"
#include "ss_viewer/model/win32_backend.h"
#include "stout/compound_file.h"
#include "stout/ole/property_set.h"

#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <objbase.h>
#include <objidl.h>
#include <optional>
#include <span>
#include <string>
#include <unordered_set>
#include <vector>

namespace ssv {

/// @brief Central application state for ss_viewer.
struct viewer_state {
    // ── File ──
    std::filesystem::path file_path;
    bool use_win32 = false;
    uint64_t file_size = 0;

    // ── Stout backend ──
    std::optional<stout::compound_file> cf;

    // ── Win32 backend ──
    IStorage *root_stg = nullptr;

    // ── Tree data ──
    entry_info root_entry;
    std::unordered_set<std::string> expanded;
    int tree_cursor = 0;
    std::vector<std::string> flat_paths;

    // ── Selection ──
    const entry_info *selected = nullptr;

    // ── Detail tabs ──
    int active_tab = 0; ///< 0=Info, 1=Hex, 2=Properties

    // ── Hex dump cache ──
    std::vector<uint8_t> hex_data; ///< Legacy bulk data (for properties parsing)
    paged_reader hex_reader;       ///< On-demand paged reader (no size cap)
    uint32_t hex_scroll = 0;
    std::string hex_cached_path;

    // ── Properties cache ──
    std::optional<stout::ole::property_set> prop_set;
    std::string prop_cached_path;

    // ── CFB metadata strings ──
    std::string version_str;
    std::string sector_str;

    // ── UI overlay state ──
    bool show_help = false;                              ///< Show keybinding help popup.
    bool show_search = false;                            ///< Show search popup (text input mode).
    std::string search_query;                            ///< Current search query text.
    int search_match_count = 0;                          ///< Number of matches found.
    int search_current_match = 0;                        ///< Index of current match (1-based).
    std::string toast_msg;                               ///< Toast message to display (empty = hidden).
    std::chrono::steady_clock::time_point toast_until{}; ///< When toast expires.

    // ── Theme ──
    bool use_dark_theme = true; ///< true = dark, false = light.

    // ── Tree panel layout ──
    uint32_t tree_panel_width = 32; ///< Width of the tree panel in columns.
    static constexpr uint32_t tree_panel_min = 16;
    static constexpr uint32_t tree_panel_max = 60;

    // ── Goto overlay (Ctrl+G) ──
    bool show_goto = false;          ///< Show goto input overlay.
    std::string goto_query;          ///< Current goto query text.
    bool goto_is_hex_offset = false; ///< true = hex offset mode, false = path mode.

    // ── Hex enhancements ──
    uint64_t hex_cursor = 0;        ///< Byte offset of hex cursor.
    bool hex_show_ascii = true;     ///< Show ASCII column in hex view.
    uint64_t hex_sel_start = 0;     ///< Start of hex selection range.
    uint64_t hex_sel_end = 0;       ///< End of hex selection range (exclusive).
    bool hex_has_selection = false; ///< Whether a hex selection is active.

    // ── Navigation history ──
    std::deque<std::string> nav_back;    ///< Back history (full_path strings).
    std::deque<std::string> nav_forward; ///< Forward history.
    static constexpr size_t nav_max = 50;

    // ── Bookmarks ──
    std::vector<std::string> bookmarks; ///< Bookmarked entry full_paths.

    // ── Dirty flag for diff-based rebuild ──
    bool dirty = true; ///< Set when state changes; cleared after widget rebuild.

    /// @brief Push current selection onto back history before navigating.
    void push_nav_history() {
        if (selected) {
            nav_back.push_back(selected->full_path);
            if (nav_back.size() > nav_max) {
                nav_back.pop_front();
            }
            nav_forward.clear();
        }
    }

    /// @brief Navigate back in history.
    void nav_go_back() {
        if (nav_back.empty()) {
            return;
        }
        if (selected) {
            nav_forward.push_back(selected->full_path);
            if (nav_forward.size() > nav_max) {
                nav_forward.pop_front();
            }
        }
        auto target = nav_back.back();
        nav_back.pop_back();
        navigate_to_path(target);
    }

    /// @brief Navigate forward in history.
    void nav_go_forward() {
        if (nav_forward.empty()) {
            return;
        }
        if (selected) {
            nav_back.push_back(selected->full_path);
            if (nav_back.size() > nav_max) {
                nav_back.pop_front();
            }
        }
        auto target = nav_forward.back();
        nav_forward.pop_back();
        navigate_to_path(target);
    }

    /// @brief Navigate to a specific entry by full_path.
    void navigate_to_path(const std::string &target) {
        for (int i = 0; i < static_cast<int>(flat_paths.size()); ++i) {
            if (flat_paths[i] == target) {
                tree_cursor = i;
                select_current();
                return;
            }
        }
    }

    /// @brief Toggle bookmark for the currently selected entry.
    void toggle_bookmark() {
        if (!selected) {
            return;
        }
        auto it = std::find(bookmarks.begin(), bookmarks.end(), selected->full_path);
        if (it != bookmarks.end()) {
            bookmarks.erase(it);
        } else {
            bookmarks.push_back(selected->full_path);
        }
        dirty = true;
    }

    /// @brief Check if an entry is bookmarked.
    [[nodiscard]] bool is_bookmarked(const std::string &path) const {
        return std::find(bookmarks.begin(), bookmarks.end(), path) != bookmarks.end();
    }

    /// @brief Ensure children of an entry are loaded (lazy loading).
    void ensure_children_loaded(entry_info &ei) {
        if (ei.children_loaded) {
            return;
        }
        if (use_win32 && root_stg) {
            load_win32_children(root_stg, ei);
        } else if (cf) {
            load_stout_children(*cf, ei);
        } else {
            ei.children_loaded = true;
        }
    }

    /// @brief Rebuild the flat path list from the tree, respecting expanded set.
    void rebuild_flat_paths() {
        flat_paths.clear();
        flatten_paths(root_entry, flat_paths, expanded);
        dirty = true;
    }

    /// @brief Update selection to match current tree_cursor position.
    void select_current() {
        if (tree_cursor >= 0 && tree_cursor < static_cast<int>(flat_paths.size())) {
            auto &path = flat_paths[tree_cursor];
            selected = find_entry(root_entry, path);
            load_hex_data();
            load_properties();
            dirty = true;
        }
    }

    /// @brief Load hex data for the currently selected stream.
    void load_hex_data() {
        if (!selected || selected->type != stout::entry_type::stream) {
            hex_data.clear();
            hex_reader.clear();
            hex_cached_path.clear();
            hex_scroll = 0;
            return;
        }
        if (hex_cached_path == selected->full_path) {
            return;
        }
        hex_cached_path = selected->full_path;
        hex_scroll = 0;

        // Open paged reader (no size cap)
        if (use_win32 && root_stg) {
            hex_reader = open_win32_reader(root_stg, *selected);
        } else if (cf) {
            hex_reader = open_stout_reader(*cf, *selected);
        } else {
            hex_reader.clear();
        }

        // Also load bulk data for property parsing (capped at 64 KB)
        if (use_win32 && root_stg) {
            hex_data = read_win32_stream(root_stg, *selected);
        } else if (cf) {
            hex_data = read_stout_stream(*cf, *selected);
        } else {
            hex_data.clear();
        }
    }

    /// @brief Load OLE property set for the currently selected property stream.
    void load_properties() {
        if (!selected || selected->type != stout::entry_type::stream || !is_property_stream(selected->name)) {
            prop_set.reset();
            prop_cached_path.clear();
            return;
        }
        if (prop_cached_path == selected->full_path) {
            return;
        }
        prop_cached_path = selected->full_path;

        if (!hex_data.empty()) {
            auto result = stout::ole::parse_property_set(std::span<const uint8_t>(hex_data));
            if (result) {
                prop_set = std::move(*result);
            } else {
                prop_set.reset();
            }
        } else {
            prop_set.reset();
        }
    }

    ~viewer_state() {
        if (root_stg) {
            root_stg->Release();
            root_stg = nullptr;
        }
    }

    viewer_state() = default;
    viewer_state(const viewer_state &) = delete;
    viewer_state &operator=(const viewer_state &) = delete;
    viewer_state(viewer_state &&) = default;
    viewer_state &operator=(viewer_state &&) = default;
};

} // namespace ssv
