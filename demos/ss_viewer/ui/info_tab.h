/**
 * @file info_tab.h
 * @brief Info tab builder — entry metadata table.
 */
#pragma once

#include "ss_viewer/model/entry_info.h"
#include "ss_viewer/ui/theme.h"
#include "ss_viewer/util/format.h"
#include "tapiru/widgets/builders.h"

#include <format>
#include <string>

namespace ssv {

/// @brief Build the Info tab table showing entry metadata.
[[nodiscard]] inline tapiru::table_builder build_info_table(const entry_info &ei) {
    tapiru::table_builder tb;
    tb.add_column("Property", {tapiru::justify::left, 16, 20});
    tb.add_column("Value", {tapiru::justify::left, 20, 60});
    tb.border(tapiru::border_style::rounded);
    tb.border_style_override(theme::info_border);
    tb.header_style(theme::info_header);

    tb.add_row({"Name", ei.name});
    tb.add_row({"Type", entry_type_str(ei.type)});
    tb.add_row({"Size", format_size(ei.size) + " (" + std::to_string(ei.size) + " bytes)"});
    if (!is_null_guid(ei.clsid)) tb.add_row({"CLSID", format_guid(ei.clsid)});
    tb.add_row({"Created", format_time(ei.creation_time)});
    tb.add_row({"Modified", format_time(ei.modified_time)});
    if (ei.state_bits != 0) tb.add_row({"State Bits", std::format("0x{:08X}", ei.state_bits)});
    tb.add_row({"Path", ei.full_path});

    return tb;
}

} // namespace ssv
