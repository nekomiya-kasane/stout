#pragma once

#include <bit>
#include <concepts>
#include <cstdint>
#include <cstring>
#include <span>

namespace stout::util {

// ── Little-endian read from byte span ──────────────────────────────────

[[nodiscard]] constexpr auto read_u16_le(std::span<const uint8_t, 2> src) noexcept -> uint16_t {
    return static_cast<uint16_t>(src[0])
         | (static_cast<uint16_t>(src[1]) << 8);
}

[[nodiscard]] constexpr auto read_u32_le(std::span<const uint8_t, 4> src) noexcept -> uint32_t {
    return static_cast<uint32_t>(src[0])
         | (static_cast<uint32_t>(src[1]) << 8)
         | (static_cast<uint32_t>(src[2]) << 16)
         | (static_cast<uint32_t>(src[3]) << 24);
}

[[nodiscard]] constexpr auto read_u64_le(std::span<const uint8_t, 8> src) noexcept -> uint64_t {
    return static_cast<uint64_t>(src[0])
         | (static_cast<uint64_t>(src[1]) << 8)
         | (static_cast<uint64_t>(src[2]) << 16)
         | (static_cast<uint64_t>(src[3]) << 24)
         | (static_cast<uint64_t>(src[4]) << 32)
         | (static_cast<uint64_t>(src[5]) << 40)
         | (static_cast<uint64_t>(src[6]) << 48)
         | (static_cast<uint64_t>(src[7]) << 56);
}

// ── Little-endian read from raw pointer (runtime convenience) ──────────

[[nodiscard]] inline auto read_u16_le(const uint8_t* src) noexcept -> uint16_t {
    return read_u16_le(std::span<const uint8_t, 2>{src, 2});
}

[[nodiscard]] inline auto read_u32_le(const uint8_t* src) noexcept -> uint32_t {
    return read_u32_le(std::span<const uint8_t, 4>{src, 4});
}

[[nodiscard]] inline auto read_u64_le(const uint8_t* src) noexcept -> uint64_t {
    return read_u64_le(std::span<const uint8_t, 8>{src, 8});
}

// ── Little-endian write to byte span ───────────────────────────────────

constexpr void write_u16_le(std::span<uint8_t, 2> dst, uint16_t val) noexcept {
    dst[0] = static_cast<uint8_t>(val);
    dst[1] = static_cast<uint8_t>(val >> 8);
}

constexpr void write_u32_le(std::span<uint8_t, 4> dst, uint32_t val) noexcept {
    dst[0] = static_cast<uint8_t>(val);
    dst[1] = static_cast<uint8_t>(val >> 8);
    dst[2] = static_cast<uint8_t>(val >> 16);
    dst[3] = static_cast<uint8_t>(val >> 24);
}

constexpr void write_u64_le(std::span<uint8_t, 8> dst, uint64_t val) noexcept {
    dst[0] = static_cast<uint8_t>(val);
    dst[1] = static_cast<uint8_t>(val >> 8);
    dst[2] = static_cast<uint8_t>(val >> 16);
    dst[3] = static_cast<uint8_t>(val >> 24);
    dst[4] = static_cast<uint8_t>(val >> 32);
    dst[5] = static_cast<uint8_t>(val >> 40);
    dst[6] = static_cast<uint8_t>(val >> 48);
    dst[7] = static_cast<uint8_t>(val >> 56);
}

// ── Little-endian write to raw pointer ─────────────────────────────────

inline void write_u16_le(uint8_t* dst, uint16_t val) noexcept {
    write_u16_le(std::span<uint8_t, 2>{dst, 2}, val);
}

inline void write_u32_le(uint8_t* dst, uint32_t val) noexcept {
    write_u32_le(std::span<uint8_t, 4>{dst, 4}, val);
}

inline void write_u64_le(uint8_t* dst, uint64_t val) noexcept {
    write_u64_le(std::span<uint8_t, 8>{dst, 8}, val);
}

} // namespace stout::util
