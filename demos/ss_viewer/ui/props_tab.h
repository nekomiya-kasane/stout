/**
 * @file props_tab.h
 * @brief Properties tab builder — OLE property set table.
 */
#pragma once

#include "ss_viewer/ui/theme.h"
#include "ss_viewer/util/format.h"
#include "stout/ole/property_set.h"
#include "stout/ole/property_set_storage.h"
#include "tapiru/widgets/builders.h"

#include <format>
#include <string>

namespace ssv {

/// @brief Build a table of OLE properties from a property_set.
[[nodiscard]] inline tapiru::table_builder build_properties_table(const stout::ole::property_set &ps) {
    tapiru::table_builder tb;
    tb.add_column("PID", {tapiru::justify::right, 6, 8});
    tb.add_column("Type", {tapiru::justify::left, 10, 14});
    tb.add_column("Value", {tapiru::justify::left, 20, 60});
    tb.border(tapiru::border_style::rounded);
    tb.border_style_override(theme::props_border);
    tb.header_style(theme::props_header);

    for (auto &section : ps.sections) {
        for (auto &[pid, prop] : section.properties) {
            if (pid == 0 || pid == 1) {
                continue; // skip dictionary and codepage
            }
            std::string value = stout::ole::property_value_to_string(prop);
            std::string type_name = stout::ole::vt_type_name(prop.type);
            tb.add_row({std::to_string(pid), type_name, value});
        }
    }
    return tb;
}

} // namespace ssv
