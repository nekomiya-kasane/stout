/**
 * @file stout_backend.h
 * @brief Stout library backend — builds entry tree and reads streams via stout API.
 */
#pragma once

#include "ss_viewer/model/entry_info.h"
#include "ss_viewer/model/paged_reader.h"
#include "stout/compound_file.h"

#include <cstdint>
#include <string>
#include <vector>

namespace ssv {

    /// @brief Recursively build an entry_info tree from a stout storage.
    [[nodiscard]] entry_info build_stout_tree(stout::storage &stg, const std::string &parent_path);

    /// @brief Build a shallow entry_info (root + first-level children only, children not recursed).
    [[nodiscard]] entry_info build_stout_tree_shallow(stout::storage &stg, const std::string &parent_path);

    /// @brief Lazily load children of a storage entry (populates ei.children, sets children_loaded).
    void load_stout_children(stout::compound_file &cf, entry_info &ei);

    /// @brief Read stream bytes by navigating the entry's full_path.
    [[nodiscard]] std::vector<uint8_t> read_stout_stream(stout::compound_file &cf, const entry_info &ei,
                                                         uint64_t max_bytes = 64 * 1024);

    /// @brief Create a paged_reader for a stout stream (no size cap, reads on demand).
    [[nodiscard]] paged_reader open_stout_reader(stout::compound_file &cf, const entry_info &ei);

} // namespace ssv
