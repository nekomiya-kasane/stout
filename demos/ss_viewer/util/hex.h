/**
 * @file hex.h
 * @brief Hex dump line formatter for ss_viewer.
 */
#pragma once

#include <cstdint>
#include <format>
#include <span>
#include <string>

namespace ssv {

/// @brief Format one 16-byte hex dump line at the given offset.
[[nodiscard]] inline std::string hex_line(std::span<const uint8_t> data, uint32_t offset) {
    std::string out = std::format("{:08X}  ", offset);

    uint32_t end = std::min(offset + 16u, static_cast<uint32_t>(data.size()));
    for (uint32_t i = offset; i < offset + 16; ++i) {
        if (i == offset + 8) {
            out += ' ';
        }
        if (i < end) {
            out += std::format("{:02X} ", data[i]);
        } else {
            out += "   ";
        }
    }
    out += " |";
    for (uint32_t i = offset; i < end; ++i) {
        uint8_t b = data[i];
        out += (b >= 0x20 && b < 0x7F) ? static_cast<char>(b) : '.';
    }
    out += '|';
    return out;
}

} // namespace ssv
