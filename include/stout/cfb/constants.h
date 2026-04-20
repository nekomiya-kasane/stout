#pragma once

#include <array>
#include <cstdint>

namespace stout::cfb {

// ── Magic signature ────────────────────────────────────────────────────

inline constexpr std::array<uint8_t, 8> signature = {
    0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1
};

// ── Byte order mark ────────────────────────────────────────────────────

inline constexpr uint16_t byte_order_le = 0xFFFE;

// ── Version numbers ────────────────────────────────────────────────────

inline constexpr uint16_t major_version_3 = 0x0003;
inline constexpr uint16_t major_version_4 = 0x0004;
inline constexpr uint16_t minor_version   = 0x003E;

// ── Sector shifts ──────────────────────────────────────────────────────

inline constexpr uint16_t sector_shift_v3 = 9;   // 512 bytes
inline constexpr uint16_t sector_shift_v4 = 12;  // 4096 bytes

inline constexpr uint32_t sector_size_v3 = 512;
inline constexpr uint32_t sector_size_v4 = 4096;

// ── Mini stream ────────────────────────────────────────────────────────

inline constexpr uint16_t mini_sector_shift  = 6;   // 64 bytes
inline constexpr uint32_t mini_sector_size   = 64;
inline constexpr uint32_t mini_stream_cutoff = 4096;

// ── Special sector IDs ─────────────────────────────────────────────────

inline constexpr uint32_t max_regsid    = 0xFFFFFFFA;
inline constexpr uint32_t difsect       = 0xFFFFFFFC;
inline constexpr uint32_t fatsect       = 0xFFFFFFFD;
inline constexpr uint32_t endofchain    = 0xFFFFFFFE;
inline constexpr uint32_t freesect      = 0xFFFFFFFF;

// ── Special stream IDs ─────────────────────────────────────────────────

inline constexpr uint32_t nostream      = 0xFFFFFFFF;

// ── Directory entry ────────────────────────────────────────────────────

inline constexpr uint32_t dir_entry_size       = 128;
inline constexpr uint32_t dir_name_max_bytes   = 64;
inline constexpr uint32_t dir_name_max_chars   = 32;  // including null terminator

// ── Header ─────────────────────────────────────────────────────────────

inline constexpr uint32_t header_size          = 512;
inline constexpr uint32_t difat_in_header      = 109;

// ── Directory entries per sector ───────────────────────────────────────

[[nodiscard]] constexpr auto dir_entries_per_sector(uint32_t sector_size) noexcept -> uint32_t {
    return sector_size / dir_entry_size;
}

// ── FAT entries per sector ─────────────────────────────────────────────

[[nodiscard]] constexpr auto fat_entries_per_sector(uint32_t sector_size) noexcept -> uint32_t {
    return sector_size / 4;
}

// ── DIFAT entries per sector (last entry is chain pointer) ─────────────

[[nodiscard]] constexpr auto difat_entries_per_sector(uint32_t sector_size) noexcept -> uint32_t {
    return (sector_size / 4) - 1;
}

} // namespace stout::cfb
