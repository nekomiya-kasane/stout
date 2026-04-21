#pragma once

#include "stout/cfb/constants.h"
#include "stout/exports.h"
#include "stout/types.h"
#include "stout/util/guid.h"

#include <array>
#include <cstdint>
#include <expected>
#include <span>

namespace stout::cfb {

    struct cfb_header {
        std::array<uint8_t, 8> signature = {};
        guid clsid = {};
        uint16_t minor_ver = minor_version;
        uint16_t major_ver = major_version_4;
        uint16_t byte_order = byte_order_le;
        uint16_t sector_shift = sector_shift_v4;
        uint16_t mini_sec_shift = mini_sector_shift;
        uint32_t total_dir_sectors = 0; // v4 only, must be 0 for v3
        uint32_t total_fat_sectors = 0;
        uint32_t first_dir_sector = endofchain;
        uint32_t transaction_sig = 0;
        uint32_t mini_stream_cutoff_size = mini_stream_cutoff;
        uint32_t first_mini_fat_sector = endofchain;
        uint32_t total_mini_fat_sectors = 0;
        uint32_t first_difat_sector = endofchain;
        uint32_t total_difat_sectors = 0;
        std::array<uint32_t, difat_in_header> difat = {};

        [[nodiscard]] auto sector_size() const noexcept -> uint32_t { return 1u << sector_shift; }

        [[nodiscard]] auto mini_sector_size_val() const noexcept -> uint32_t { return 1u << mini_sec_shift; }

        [[nodiscard]] auto is_v3() const noexcept -> bool { return major_ver == major_version_3; }
        [[nodiscard]] auto is_v4() const noexcept -> bool { return major_ver == major_version_4; }
    };

    // Initialize a default header for a new compound file
    [[nodiscard]] STOUT_API auto make_default_header(cfb_version version) noexcept -> cfb_header;

    // Parse a 512-byte header from raw bytes
    [[nodiscard]] STOUT_API auto parse_header(std::span<const uint8_t, header_size> data) noexcept
        -> std::expected<cfb_header, error>;

    // Serialize a header to 512 bytes
    STOUT_API void serialize_header(const cfb_header &hdr, std::span<uint8_t, header_size> out) noexcept;

    // Validate a parsed header for consistency
    [[nodiscard]] STOUT_API auto validate_header(const cfb_header &hdr) noexcept -> std::expected<void, error>;

} // namespace stout::cfb
