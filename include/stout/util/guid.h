#pragma once

#include "stout/exports.h"
#include <array>
#include <compare>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>

namespace stout {

struct guid {
    uint32_t data1 = 0;
    uint16_t data2 = 0;
    uint16_t data3 = 0;
    std::array<uint8_t, 8> data4 = {};

    [[nodiscard]] constexpr bool is_null() const noexcept {
        return data1 == 0 && data2 == 0 && data3 == 0
            && data4[0] == 0 && data4[1] == 0 && data4[2] == 0 && data4[3] == 0
            && data4[4] == 0 && data4[5] == 0 && data4[6] == 0 && data4[7] == 0;
    }

    constexpr auto operator<=>(const guid&) const noexcept = default;
    constexpr bool operator==(const guid&) const noexcept = default;
};

inline constexpr guid guid_null = {};

// Parse from string like "{D5CDD502-2E9C-101B-9397-08002B2CF9AE}" or without braces
[[nodiscard]] STOUT_API auto guid_parse(std::string_view str) noexcept -> std::optional<guid>;

// Format as "{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}"
[[nodiscard]] STOUT_API auto guid_to_string(const guid& g) -> std::string;

// Generate a random v4 GUID
[[nodiscard]] STOUT_API auto guid_generate() -> guid;

// Serialize/deserialize in MS mixed-endian format (data1/2/3 LE, data4 raw)
STOUT_API void guid_write_le(uint8_t* dst, const guid& g) noexcept;
[[nodiscard]] STOUT_API auto guid_read_le(const uint8_t* src) noexcept -> guid;

} // namespace stout

template<>
struct std::formatter<stout::guid> {
    constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

    auto format(const stout::guid& g, std::format_context& ctx) const {
        return std::format_to(ctx.out(),
            "{{{:08X}-{:04X}-{:04X}-{:02X}{:02X}-{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}}}",
            g.data1, g.data2, g.data3,
            g.data4[0], g.data4[1],
            g.data4[2], g.data4[3], g.data4[4], g.data4[5], g.data4[6], g.data4[7]);
    }
};
