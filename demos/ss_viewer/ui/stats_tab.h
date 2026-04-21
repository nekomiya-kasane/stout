/**
 * @file stats_tab.h
 * @brief Entry statistics tab — total entries, sizes, depth, type breakdown.
 */
#pragma once

#include "ss_viewer/model/entry_info.h"
#include "ss_viewer/util/format.h"
#include "tapiru/widgets/builders.h"

#include <algorithm>
#include <cstdint>
#include <format>
#include <string>

namespace ssv {

/// @brief Collected statistics about the CFB file.
struct cfb_stats {
    uint32_t total_entries = 0;
    uint32_t storage_count = 0;
    uint32_t stream_count = 0;
    uint64_t total_stream_bytes = 0;
    uint64_t largest_stream = 0;
    std::string largest_stream_name;
    uint32_t max_depth = 0;
};

/// @brief Recursively collect statistics from the entry tree.
inline void collect_stats(const entry_info &ei, cfb_stats &s, uint32_t depth = 0) {
    ++s.total_entries;
    s.max_depth = std::max(s.max_depth, depth);

    if (ei.type == stout::entry_type::storage || ei.type == stout::entry_type::root) {
        ++s.storage_count;
    } else {
        ++s.stream_count;
        s.total_stream_bytes += ei.size;
        if (ei.size > s.largest_stream) {
            s.largest_stream = ei.size;
            s.largest_stream_name = ei.name;
        }
    }
    for (auto &c : ei.children) {
        collect_stats(c, s, depth + 1);
    }
}

/// @brief Build the Stats tab content.
[[nodiscard]] inline tapiru::rows_builder build_stats_tab(const entry_info &root) {
    cfb_stats s;
    collect_stats(root, s);

    tapiru::rows_builder rows;
    rows.gap(0);

    rows.add(tapiru::text_builder("[bold bright_cyan]File Statistics[/]"));
    rows.add(tapiru::text_builder(""));
    rows.add(tapiru::text_builder(std::format("  Total entries:     [bold]{}[/]", s.total_entries)));
    rows.add(tapiru::text_builder(std::format("  Storages:          [bold]{}[/]", s.storage_count)));
    rows.add(tapiru::text_builder(std::format("  Streams:           [bold]{}[/]", s.stream_count)));
    rows.add(tapiru::text_builder(std::format("  Total stream data: [bold]{}[/]", format_size(s.total_stream_bytes))));
    rows.add(tapiru::text_builder(std::format("  Largest stream:    [bold]{}[/] ({})", format_size(s.largest_stream),
                                              s.largest_stream_name.empty() ? "none" : s.largest_stream_name)));
    rows.add(tapiru::text_builder(std::format("  Max nesting depth: [bold]{}[/]", s.max_depth)));

    return rows;
}

} // namespace ssv
