#include "stout/util/guid.h"
#include "stout/util/endian.h"
#include <algorithm>
#include <charconv>
#include <random>

namespace stout {

namespace {

auto hex_nibble(char ch) noexcept -> int {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

auto hex_byte(char hi, char lo) noexcept -> int {
    int h = hex_nibble(hi);
    int l = hex_nibble(lo);
    if (h < 0 || l < 0) return -1;
    return (h << 4) | l;
}

auto parse_hex_u32(std::string_view s) noexcept -> std::optional<uint32_t> {
    if (s.size() != 8) return std::nullopt;
    uint32_t val = 0;
    for (char ch : s) {
        int n = hex_nibble(ch);
        if (n < 0) return std::nullopt;
        val = (val << 4) | static_cast<uint32_t>(n);
    }
    return val;
}

auto parse_hex_u16(std::string_view s) noexcept -> std::optional<uint16_t> {
    if (s.size() != 4) return std::nullopt;
    uint16_t val = 0;
    for (char ch : s) {
        int n = hex_nibble(ch);
        if (n < 0) return std::nullopt;
        val = static_cast<uint16_t>((val << 4) | static_cast<uint16_t>(n));
    }
    return val;
}

} // anonymous namespace

auto guid_parse(std::string_view str) noexcept -> std::optional<guid> {
    // Strip optional braces
    if (str.size() >= 2 && str.front() == '{' && str.back() == '}') {
        str = str.substr(1, str.size() - 2);
    }

    // Expected: XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX (36 chars)
    if (str.size() != 36) return std::nullopt;
    if (str[8] != '-' || str[13] != '-' || str[18] != '-' || str[23] != '-')
        return std::nullopt;

    auto d1 = parse_hex_u32(str.substr(0, 8));
    auto d2 = parse_hex_u16(str.substr(9, 4));
    auto d3 = parse_hex_u16(str.substr(14, 4));
    if (!d1 || !d2 || !d3) return std::nullopt;

    guid g;
    g.data1 = *d1;
    g.data2 = *d2;
    g.data3 = *d3;

    // data4[0..1] from positions 19-22 (XXXX)
    auto sv = str.substr(19, 4);
    for (int i = 0; i < 2; ++i) {
        int b = hex_byte(sv[i * 2], sv[i * 2 + 1]);
        if (b < 0) return std::nullopt;
        g.data4[i] = static_cast<uint8_t>(b);
    }

    // data4[2..7] from positions 24-35 (XXXXXXXXXXXX)
    sv = str.substr(24, 12);
    for (int i = 0; i < 6; ++i) {
        int b = hex_byte(sv[i * 2], sv[i * 2 + 1]);
        if (b < 0) return std::nullopt;
        g.data4[2 + i] = static_cast<uint8_t>(b);
    }

    return g;
}

auto guid_to_string(const guid& g) -> std::string {
    return std::format("{}", g);
}

auto guid_generate() -> guid {
    static thread_local std::mt19937_64 rng{std::random_device{}()};

    guid g;
    std::uniform_int_distribution<uint32_t> dist32;
    std::uniform_int_distribution<uint32_t> dist8(0, 255);

    g.data1 = dist32(rng);
    g.data2 = static_cast<uint16_t>(dist32(rng));
    g.data3 = static_cast<uint16_t>((dist32(rng) & 0x0FFF) | 0x4000); // version 4
    g.data4[0] = static_cast<uint8_t>((dist8(rng) & 0x3F) | 0x80);   // variant 1
    for (int i = 1; i < 8; ++i) {
        g.data4[i] = static_cast<uint8_t>(dist8(rng));
    }

    return g;
}

void guid_write_le(uint8_t* dst, const guid& g) noexcept {
    util::write_u32_le(dst, g.data1);
    util::write_u16_le(dst + 4, g.data2);
    util::write_u16_le(dst + 6, g.data3);
    std::copy_n(g.data4.begin(), 8, dst + 8);
}

auto guid_read_le(const uint8_t* src) noexcept -> guid {
    guid g;
    g.data1 = util::read_u32_le(src);
    g.data2 = util::read_u16_le(src + 4);
    g.data3 = util::read_u16_le(src + 6);
    std::copy_n(src + 8, 8, g.data4.begin());
    return g;
}

} // namespace stout
