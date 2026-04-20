/**
 * @file format.h
 * @brief Formatting utilities for ss_viewer — sizes, times, GUIDs, entry types.
 */
#pragma once

#include <chrono>
#include <cstdint>
#include <format>
#include <string>

#include "stout/types.h"
#include "stout/util/guid.h"

namespace ssv {

/// @brief Format a byte count as a human-readable string (e.g. "1.5 KB").
[[nodiscard]] inline std::string format_size(uint64_t bytes) {
    if (bytes < 1024) return std::format("{} B", bytes);
    if (bytes < 1024 * 1024) return std::format("{:.1f} KB", bytes / 1024.0);
    if (bytes < 1024ULL * 1024 * 1024) return std::format("{:.1f} MB", bytes / (1024.0 * 1024.0));
    return std::format("{:.2f} GB", bytes / (1024.0 * 1024.0 * 1024.0));
}

/// @brief Format a file_time as "YYYY-MM-DD HH:MM:SS", or "(none)" if zero.
[[nodiscard]] inline std::string format_time(stout::file_time ft) {
    if (ft == stout::file_time{}) return "(none)";
    auto sys = std::chrono::system_clock::to_time_t(ft);
    char buf[64];
    struct tm tm_buf;
    localtime_s(&tm_buf, &sys);
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_buf);
    return buf;
}

/// @brief Format a GUID as "{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}".
[[nodiscard]] inline std::string format_guid(const stout::guid& g) {
    return std::format("{{{:08X}-{:04X}-{:04X}-{:02X}{:02X}-{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}}}",
        g.data1, g.data2, g.data3,
        g.data4[0], g.data4[1], g.data4[2], g.data4[3],
        g.data4[4], g.data4[5], g.data4[6], g.data4[7]);
}

/// @brief Return a human-readable name for an entry_type.
[[nodiscard]] inline std::string entry_type_str(stout::entry_type t) {
    switch (t) {
        case stout::entry_type::root:    return "Root Storage";
        case stout::entry_type::storage: return "Storage";
        case stout::entry_type::stream:  return "Stream";
        default:                         return "Unknown";
    }
}

/// @brief Check whether a GUID is all zeros.
[[nodiscard]] inline bool is_null_guid(const stout::guid& g) {
    return g.data1 == 0 && g.data2 == 0 && g.data3 == 0 &&
           g.data4[0] == 0 && g.data4[1] == 0 && g.data4[2] == 0 && g.data4[3] == 0 &&
           g.data4[4] == 0 && g.data4[5] == 0 && g.data4[6] == 0 && g.data4[7] == 0;
}

/// @brief Check whether a stream name is a property stream (starts with 0x05).
[[nodiscard]] inline bool is_property_stream(const std::string& name) {
    return !name.empty() && name[0] == '\x05';
}

} // namespace ssv
