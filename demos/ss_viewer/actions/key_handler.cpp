/**
 * @file key_handler.cpp
 * @brief Keyboard event handler implementation.
 */
#include "ss_viewer/actions/key_handler.h"

#include "ss_viewer/model/entry_info.h"
#include "ss_viewer/util/format.h"
#include "stout/ole/property_set.h"
#include "stout/ole/property_set_storage.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <format>
#include <fstream>

namespace ssv {

using namespace tapiru;

/// @brief Copy text to Win32 clipboard.
static void copy_to_clipboard(const std::string &text) {
    if (!OpenClipboard(nullptr)) return;
    EmptyClipboard();
    HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
    if (hg) {
        auto *dst = static_cast<char *>(GlobalLock(hg));
        if (dst) {
            std::memcpy(dst, text.c_str(), text.size() + 1);
            GlobalUnlock(hg);
            SetClipboardData(CF_TEXT, hg);
        }
    }
    CloseClipboard();
}

/// @brief Build a copyable string for the current selection context.
static std::string build_copy_text(const viewer_state &st) {
    if (!st.selected) return {};

    // On Hex tab, copy the hex view of current page
    if (st.active_tab == 1 && st.selected->type == stout::entry_type::stream) {
        return st.selected->full_path + " (stream, " + format_size(st.selected->size) + ")";
    }
    // On Properties tab, copy property info
    if (st.active_tab == 2 && st.prop_set) {
        std::string out = st.selected->full_path + "\n";
        for (auto &sec : st.prop_set->sections) {
            for (auto &[id, prop] : sec.properties) {
                out += std::format("  0x{:04X} = {}\n", id, stout::ole::property_value_to_string(prop));
            }
        }
        return out;
    }
    // Default: copy entry path + type + size
    return st.selected->full_path + " (" + entry_type_str(st.selected->type) + ", " + format_size(st.selected->size) +
           ")";
}

/// @brief Case-insensitive substring search.
static bool icontains(const std::string &haystack, const std::string &needle) {
    if (needle.empty()) return false;
    auto it = std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end(), [](char a, char b) {
        return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
    });
    return it != haystack.end();
}

/// @brief Count matching entries and jump to the next match after current cursor.
static void search_jump_next(viewer_state &st) {
    if (st.search_query.empty()) {
        st.search_match_count = 0;
        st.search_current_match = 0;
        return;
    }
    // Count all matches
    int count = 0;
    int first_after = -1;
    int first_overall = -1;
    for (int i = 0; i < static_cast<int>(st.flat_paths.size()); ++i) {
        auto *e = find_entry(st.root_entry, st.flat_paths[i]);
        if (e && icontains(e->name, st.search_query)) {
            ++count;
            if (first_overall < 0) first_overall = i;
            if (first_after < 0 && i > st.tree_cursor) first_after = i;
        }
    }
    st.search_match_count = count;
    if (count == 0) {
        st.search_current_match = 0;
        return;
    }
    // Jump to next match (wrap around)
    int target = (first_after >= 0) ? first_after : first_overall;
    st.tree_cursor = target;
    st.select_current();
    // Compute current match index
    int idx = 0;
    for (int i = 0; i <= target; ++i) {
        auto *e = find_entry(st.root_entry, st.flat_paths[i]);
        if (e && icontains(e->name, st.search_query)) ++idx;
    }
    st.search_current_match = idx;
}

/// @brief Recursively export an entry_info tree to a JSON-like string.
static void export_entry_json(const entry_info &ei, std::string &out, int indent) {
    std::string pad(indent * 2, ' ');
    out += pad + "{\n";
    out += pad + "  \"name\": \"" + ei.name + "\",\n";
    out += pad + "  \"type\": \"" + entry_type_str(ei.type) + "\",\n";
    out += pad + "  \"size\": " + std::to_string(ei.size) + ",\n";
    out += pad + "  \"path\": \"" + ei.full_path + "\"";
    if (!ei.children.empty()) {
        out += ",\n" + pad + "  \"children\": [\n";
        for (size_t i = 0; i < ei.children.size(); ++i) {
            export_entry_json(ei.children[i], out, indent + 2);
            if (i + 1 < ei.children.size()) out += ",";
            out += "\n";
        }
        out += pad + "  ]\n";
    } else {
        out += "\n";
    }
    out += pad + "}";
}

/// @brief Export the tree to a JSON file next to the CFB file.
static void export_tree_json(viewer_state &st) {
    std::string json;
    export_entry_json(st.root_entry, json, 0);
    json += "\n";

    auto out_path = st.file_path;
    out_path.replace_extension(".json");
    std::ofstream ofs(out_path, std::ios::binary);
    if (ofs) {
        ofs.write(json.data(), static_cast<std::streamsize>(json.size()));
        st.toast_msg = "Exported to " + out_path.filename().string();
    } else {
        st.toast_msg = "Export failed";
    }
    st.toast_until = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    st.dirty = true;
}

/// @brief Handle goto overlay input: navigate to entry path or hex offset.
static void goto_execute(viewer_state &st) {
    if (st.goto_query.empty()) return;

    if (st.goto_is_hex_offset) {
        // Parse hex offset
        uint64_t offset = 0;
        try {
            if (st.goto_query.starts_with("0x") || st.goto_query.starts_with("0X"))
                offset = std::stoull(st.goto_query, nullptr, 16);
            else
                offset = std::stoull(st.goto_query, nullptr, 0);
        } catch (...) {
            st.toast_msg = "Invalid offset";
            st.toast_until = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            st.dirty = true;
            return;
        }
        st.hex_scroll = static_cast<uint32_t>(offset / 16);
        st.hex_cursor = offset;
        st.dirty = true;
    } else {
        // Search for entry by name (case-insensitive substring)
        for (int i = 0; i < static_cast<int>(st.flat_paths.size()); ++i) {
            auto *e = find_entry(st.root_entry, st.flat_paths[i]);
            if (e && icontains(e->full_path, st.goto_query)) {
                st.push_nav_history();
                st.tree_cursor = i;
                st.select_current();
                break;
            }
        }
    }
}

bool handle_key(const key_event &ke, viewer_state &st, classic_app &app) {
    // ── Goto overlay mode: intercept all keys ──
    if (st.show_goto) {
        if (ke.key == special_key::escape) {
            st.show_goto = false;
            st.dirty = true;
            return true;
        }
        if (ke.key == special_key::enter) {
            goto_execute(st);
            st.show_goto = false;
            st.dirty = true;
            return true;
        }
        if (ke.key == special_key::backspace) {
            if (!st.goto_query.empty()) st.goto_query.pop_back();
            st.dirty = true;
            return true;
        }
        if (ke.codepoint >= 32 && ke.codepoint < 127 && ke.mods == key_mod::none) {
            st.goto_query += static_cast<char>(ke.codepoint);
            st.dirty = true;
            return true;
        }
        return true;
    }

    // ── Search mode: intercept all keys ──
    if (st.show_search) {
        if (ke.key == special_key::escape) {
            st.show_search = false;
            st.dirty = true;
            return true;
        }
        if (ke.key == special_key::enter) {
            search_jump_next(st);
            st.dirty = true;
            return true;
        }
        if (ke.key == special_key::backspace) {
            if (!st.search_query.empty()) {
                st.search_query.pop_back();
                st.search_match_count = 0;
                st.search_current_match = 0;
            }
            st.dirty = true;
            return true;
        }
        // Printable character → append to query
        if (ke.codepoint >= 32 && ke.codepoint < 127 && ke.mods == key_mod::none) {
            st.search_query += static_cast<char>(ke.codepoint);
            st.dirty = true;
            return true;
        }
        return true; // consume all keys in search mode
    }

    // Help popup toggle
    if (ke.key == special_key::f1 || ke.codepoint == '?') {
        st.show_help = !st.show_help;
        st.dirty = true;
        return true;
    }
    // Dismiss help on Escape before quit
    if (st.show_help && ke.key == special_key::escape) {
        st.show_help = false;
        st.dirty = true;
        return true;
    }

    // Quit
    if (ke.codepoint == 'q' || ke.codepoint == 'Q' || ke.key == special_key::escape) {
        app.quit();
        return true;
    }

    // Search mode (Ctrl+F)
    if (ke.codepoint == 'f' && ke.mods == key_mod::ctrl) {
        st.show_search = true;
        st.search_query.clear();
        st.search_match_count = 0;
        st.search_current_match = 0;
        st.dirty = true;
        return true;
    }

    // Clipboard copy (Ctrl+C)
    if (ke.codepoint == 'c' && ke.mods == key_mod::ctrl) {
        auto text = build_copy_text(st);
        if (!text.empty()) {
            copy_to_clipboard(text);
            st.toast_msg = "Copied to clipboard";
            st.toast_until = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            st.dirty = true;
        }
        return true;
    }

    // Goto (Ctrl+G)
    if (ke.codepoint == 'g' && ke.mods == key_mod::ctrl) {
        st.show_goto = true;
        st.goto_query.clear();
        st.goto_is_hex_offset = (st.active_tab == 1); // hex tab → offset mode
        st.dirty = true;
        return true;
    }

    // Export JSON (Ctrl+E)
    if (ke.codepoint == 'e' && ke.mods == key_mod::ctrl) {
        export_tree_json(st);
        return true;
    }

    // Toggle bookmark (Ctrl+B)
    if (ke.codepoint == 'b' && ke.mods == key_mod::ctrl) {
        st.toggle_bookmark();
        if (st.selected) {
            st.toast_msg = st.is_bookmarked(st.selected->full_path) ? "Bookmarked" : "Bookmark removed";
            st.toast_until = std::chrono::steady_clock::now() + std::chrono::seconds(1);
        }
        return true;
    }

    // Navigation history: Alt+Left = back, Alt+Right = forward
    if (ke.key == special_key::left && ke.mods == key_mod::alt) {
        st.nav_go_back();
        return true;
    }
    if (ke.key == special_key::right && ke.mods == key_mod::alt) {
        st.nav_go_forward();
        return true;
    }

    // Theme toggle
    if (ke.codepoint == 't' && ke.mods == key_mod::none) {
        st.use_dark_theme = !st.use_dark_theme;
        st.toast_msg = st.use_dark_theme ? "Dark theme" : "Light theme";
        st.toast_until = std::chrono::steady_clock::now() + std::chrono::seconds(1);
        st.dirty = true;
        return true;
    }

    // Tree panel resize
    if (ke.codepoint == '+' || ke.codepoint == '=') {
        if (st.tree_panel_width < viewer_state::tree_panel_max) {
            st.tree_panel_width += 2;
            st.dirty = true;
        }
        return true;
    }
    if (ke.codepoint == '-' && ke.mods == key_mod::none) {
        if (st.tree_panel_width > viewer_state::tree_panel_min) {
            st.tree_panel_width -= 2;
            st.dirty = true;
        }
        return true;
    }

    // Hex ASCII column toggle ('a' on hex tab)
    if (ke.codepoint == 'a' && st.active_tab == 1 && ke.mods == key_mod::none) {
        st.hex_show_ascii = !st.hex_show_ascii;
        st.dirty = true;
        return true;
    }

    // Tab switching (4 tabs: Info, Hex, Properties, Stats)
    if (ke.key == special_key::tab) {
        if (ke.mods == key_mod::shift)
            st.active_tab = (st.active_tab + 3) % 4;
        else
            st.active_tab = (st.active_tab + 1) % 4;
        st.dirty = true;
        return true;
    }

    // Number keys for tabs
    if (ke.codepoint == '1') {
        st.active_tab = 0;
        st.dirty = true;
        return true;
    }
    if (ke.codepoint == '2') {
        st.active_tab = 1;
        st.dirty = true;
        return true;
    }
    if (ke.codepoint == '3') {
        st.active_tab = 2;
        st.dirty = true;
        return true;
    }
    if (ke.codepoint == '4') {
        st.active_tab = 3;
        st.dirty = true;
        return true;
    }

    // Tree navigation (with history push on cursor change)
    if (ke.key == special_key::up) {
        if (st.tree_cursor > 0) {
            st.push_nav_history();
            --st.tree_cursor;
            st.select_current();
        }
        return true;
    }
    if (ke.key == special_key::down) {
        if (st.tree_cursor + 1 < static_cast<int>(st.flat_paths.size())) {
            st.push_nav_history();
            ++st.tree_cursor;
            st.select_current();
        }
        return true;
    }

    // Home / End for tree
    if (ke.key == special_key::home) {
        if (st.tree_cursor != 0) {
            st.push_nav_history();
            st.tree_cursor = 0;
            st.select_current();
        }
        return true;
    }
    if (ke.key == special_key::end) {
        int last = static_cast<int>(st.flat_paths.size()) - 1;
        if (st.tree_cursor != last && last >= 0) {
            st.push_nav_history();
            st.tree_cursor = last;
            st.select_current();
        }
        return true;
    }

    // Enter / Right: expand storage or select stream
    if (ke.key == special_key::enter || ke.key == special_key::right) {
        if (st.selected) {
            if (st.selected->type == stout::entry_type::storage || st.selected->type == stout::entry_type::root) {
                // Lazy-load children on first expand
                auto *mutable_entry = const_cast<entry_info *>(st.selected);
                st.ensure_children_loaded(*mutable_entry);
                st.expanded.insert(tree_label(*st.selected));
                st.rebuild_flat_paths();
            }
        }
        return true;
    }

    // Left / Backspace: collapse or go to parent
    if (ke.key == special_key::left || ke.key == special_key::backspace) {
        if (st.selected) {
            // If expanded, collapse
            if (st.expanded.count(tree_label(*st.selected))) {
                st.expanded.erase(tree_label(*st.selected));
                st.rebuild_flat_paths();
                // Clamp cursor
                if (st.tree_cursor >= static_cast<int>(st.flat_paths.size()))
                    st.tree_cursor = static_cast<int>(st.flat_paths.size()) - 1;
                st.select_current();
            } else {
                // Go to parent
                auto pos = st.selected->full_path.rfind('/');
                if (pos != std::string::npos) {
                    std::string parent = st.selected->full_path.substr(0, pos);
                    for (int i = 0; i < static_cast<int>(st.flat_paths.size()); ++i) {
                        if (st.flat_paths[i] == parent) {
                            st.tree_cursor = i;
                            st.select_current();
                            break;
                        }
                    }
                }
            }
        }
        return true;
    }

    // Hex scroll (uses paged_reader total_lines — no size cap)
    if (ke.key == special_key::page_down && st.active_tab == 1) {
        uint32_t total_lines = st.hex_reader.total_lines();
        uint32_t page = 20;
        if (st.hex_scroll + page < total_lines)
            st.hex_scroll += page;
        else if (total_lines > 0)
            st.hex_scroll = total_lines - 1;
        st.dirty = true;
        return true;
    }
    if (ke.key == special_key::page_up && st.active_tab == 1) {
        uint32_t page = 20;
        if (st.hex_scroll >= page)
            st.hex_scroll -= page;
        else
            st.hex_scroll = 0;
        st.dirty = true;
        return true;
    }

    // Expand all / Collapse all shortcuts
    if (ke.codepoint == 'E' || ke.codepoint == 'e') {
        expand_all(st.root_entry, st.expanded);
        st.rebuild_flat_paths();
        return true; // dirty set by rebuild_flat_paths
    }
    if (ke.codepoint == 'C' || ke.codepoint == 'c') {
        st.expanded.clear();
        st.expanded.insert(tree_label(st.root_entry));
        st.rebuild_flat_paths();
        st.tree_cursor = 0;
        st.select_current();
        return true; // dirty set by select_current
    }

    return false;
}

} // namespace ssv
