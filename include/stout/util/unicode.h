#pragma once

#include "stout/exports.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace stout::util {

    // Convert UTF-8 string to UTF-16LE byte array (as used in CFB directory entries)
    [[nodiscard]] STOUT_API auto utf8_to_utf16le(std::string_view utf8) -> std::u16string;

    // Convert UTF-16LE string to UTF-8
    [[nodiscard]] STOUT_API auto utf16le_to_utf8(std::u16string_view utf16) -> std::string;

    // Convert UTF-16LE raw bytes (as stored in directory entry) to UTF-8
    // byte_count includes the null terminator bytes
    [[nodiscard]] STOUT_API auto dir_name_to_utf8(const uint8_t *name_bytes, uint16_t byte_count) -> std::string;

    // Convert UTF-8 name to directory entry format (UTF-16LE bytes + byte count)
    // Returns false if name is too long (> 31 chars) or contains illegal chars
    struct dir_name_result {
        uint8_t bytes[64] = {};
        uint16_t byte_count = 0;
    };
    [[nodiscard]] STOUT_API auto utf8_to_dir_name(std::string_view name) -> std::optional<dir_name_result>;

    // CFB case-insensitive comparison for directory entry names
    // Rules: compare by length first (shorter < longer), then uppercase codepoint-by-codepoint
    // Returns <0, 0, >0 like strcmp
    [[nodiscard]] STOUT_API auto cfb_name_compare(std::u16string_view a, std::u16string_view b) noexcept -> int;

    // Simple uppercase for a single UTF-16 code point (CFB rules)
    [[nodiscard]] STOUT_API auto cfb_toupper(char16_t ch) noexcept -> char16_t;

    // Check if a name contains illegal characters for CFB ('/', '\\', ':', '!')
    [[nodiscard]] STOUT_API auto cfb_name_is_valid(std::string_view name) noexcept -> bool;

} // namespace stout::util
