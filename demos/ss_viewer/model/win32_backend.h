/**
 * @file win32_backend.h
 * @brief Win32 IStorage backend — builds entry tree and reads streams via COM.
 */
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "ss_viewer/model/entry_info.h"
#include "ss_viewer/model/paged_reader.h"

#include <Windows.h>
#include <cstdint>
#include <objbase.h>
#include <objidl.h>
#include <string>
#include <vector>

namespace ssv {

    /// @brief Recursively build an entry_info tree from a Win32 IStorage.
    [[nodiscard]] entry_info build_win32_tree(IStorage *stg, const std::string &parent_path, const std::string &name);

    /// @brief Build a shallow entry_info (root + first-level children only, children not recursed).
    [[nodiscard]] entry_info build_win32_tree_shallow(IStorage *stg, const std::string &parent_path,
                                                      const std::string &name);

    /// @brief Lazily load children of a storage entry (populates ei.children, sets children_loaded).
    void load_win32_children(IStorage *root_stg, entry_info &ei);

    /// @brief Read stream bytes by navigating the entry's full_path via IStorage/IStream.
    [[nodiscard]] std::vector<uint8_t> read_win32_stream(IStorage *root_stg, const entry_info &ei,
                                                         uint64_t max_bytes = 64 * 1024);

    /// @brief Create a paged_reader for a Win32 IStream (no size cap, reads on demand).
    [[nodiscard]] paged_reader open_win32_reader(IStorage *root_stg, const entry_info &ei);

} // namespace ssv
