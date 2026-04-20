/**
 * @file hex_tab.h
 * @brief Hex tab builder — hex dump view using paged_reader (no size cap).
 */
#pragma once

#include "ss_viewer/model/viewer_state.h"
#include "ss_viewer/util/hex.h"
#include "tapiru/widgets/builders.h"
#include "tapiru/widgets/virtual_list.h"

#include <algorithm>
#include <cstdint>
#include <format>
#include <string>

namespace ssv {

/// @brief Format one hex line from a paged_reader at the given line index.
[[nodiscard]] inline std::string hex_line_paged(paged_reader &reader, uint32_t line_idx) {
    uint64_t offset = static_cast<uint64_t>(line_idx) * 16;
    std::string out = std::format("{:08X}  ", static_cast<uint32_t>(offset));

    uint8_t buf[16] = {};
    uint32_t n = reader.read_hex_line(line_idx, buf);

    for (uint32_t i = 0; i < 16; ++i) {
        if (i == 8) out += ' ';
        if (i < n)
            out += std::format("{:02X} ", buf[i]);
        else
            out += "   ";
    }
    out += " |";
    for (uint32_t i = 0; i < n; ++i) {
        out += (buf[i] >= 0x20 && buf[i] < 0x7F) ? static_cast<char>(buf[i]) : '.';
    }
    out += '|';
    return out;
}

/// @brief Build the Hex tab content using virtual_list_builder + paged_reader.
///
/// Only visible lines are constructed; the paged_reader fetches 4 KB pages
/// on demand so there is no size cap on the stream.
[[nodiscard]] inline tapiru::rows_builder build_hex_tab(viewer_state &st, int viewport_h) {
    tapiru::rows_builder hex_rows;
    hex_rows.gap(0);

    // Header
    hex_rows.add(tapiru::text_builder(
        "[bold bright_cyan]Offset    00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F  |ASCII           |[/]"));

    uint32_t total_lines = st.hex_reader.total_lines();
    uint32_t vis = static_cast<uint32_t>(std::max(viewport_h - 6, 4));

    // Use virtual_list_builder to only construct visible hex lines
    auto vlist = tapiru::virtual_list_builder(total_lines, vis)
                     .scroll_offset(st.hex_scroll)
                     .item_builder([&st](uint32_t idx) -> tapiru::text_builder {
                         return tapiru::text_builder("[green]" + hex_line_paged(st.hex_reader, idx) + "[/]");
                     });
    hex_rows.add(std::move(vlist));

    // Scroll indicator
    uint32_t end = std::min(st.hex_scroll + vis, total_lines);
    if (total_lines > vis) {
        hex_rows.add(
            tapiru::text_builder(std::format("[dim]Lines {}-{} of {} | {} (PgUp/PgDn to scroll)[/]", st.hex_scroll + 1,
                                             end, total_lines, format_size(st.hex_reader.total_size()))));
    }

    return hex_rows;
}

} // namespace ssv
