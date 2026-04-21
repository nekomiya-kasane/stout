/**
 * @file entry_info.h
 * @brief Unified entry metadata and tree helpers for ss_viewer.
 */
#pragma once

#include "ss_viewer/util/format.h"
#include "stout/types.h"
#include "stout/util/guid.h"
#include "tapiru/widgets/tree_view.h"

#include <string>
#include <unordered_set>
#include <vector>

namespace ssv {

/// @brief Unified metadata for a single entry (storage or stream) from either backend.
struct entry_info {
    std::string name;
    stout::entry_type type = stout::entry_type::unknown;
    uint64_t size = 0;
    stout::guid clsid = {};
    stout::file_time creation_time = {};
    stout::file_time modified_time = {};
    uint32_t state_bits = 0;
    std::string full_path; ///< e.g. "Root Entry/Storage1/Stream1"
    std::vector<entry_info> children;
    bool children_loaded = false; ///< True if children have been populated (for lazy loading).
};

/// @brief Build a display label for a tree node (e.g. "[+] Root Entry").
[[nodiscard]] inline std::string tree_label(const entry_info &ei) {
    if (ei.type == stout::entry_type::root || ei.type == stout::entry_type::storage) {
        return "[+] " + ei.name;
    } else if (is_property_stream(ei.name)) {
        return "[P] " + ei.name;
    } else {
        return "[F] " + ei.name;
    }
}

/// @brief Convert an entry_info tree to a tapiru tree_node tree.
[[nodiscard]] inline tapiru::tree_node to_tree_node(const entry_info &ei) {
    tapiru::tree_node n;
    n.label = tree_label(ei);
    for (auto &c : ei.children) {
        n.children.push_back(to_tree_node(c));
    }
    return n;
}

/// @brief Build a flat list of paths in tree DFS order, matching tree_view cursor indexing.
inline void flatten_paths(const entry_info &ei, std::vector<std::string> &out,
                          const std::unordered_set<std::string> &expanded) {
    out.push_back(ei.full_path);
    bool is_expanded = expanded.count(tree_label(ei)) > 0;
    if (is_expanded) {
        for (auto &c : ei.children) {
            flatten_paths(c, out, expanded);
        }
    }
}

/// @brief Find an entry_info by its full_path, or nullptr if not found.
[[nodiscard]] inline const entry_info *find_entry(const entry_info &root, const std::string &path) {
    if (root.full_path == path) {
        return &root;
    }
    for (auto &c : root.children) {
        auto *found = find_entry(c, path);
        if (found) {
            return found;
        }
    }
    return nullptr;
}

/// @brief Recursively expand all storage nodes.
inline void expand_all(const entry_info &ei, std::unordered_set<std::string> &expanded) {
    if (ei.type == stout::entry_type::root || ei.type == stout::entry_type::storage) {
        expanded.insert(tree_label(ei));
        for (auto &c : ei.children) {
            expand_all(c, expanded);
        }
    }
}

} // namespace ssv
