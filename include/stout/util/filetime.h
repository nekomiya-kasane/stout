#pragma once

#include "stout/exports.h"
#include "stout/types.h"
#include <chrono>
#include <cstdint>

namespace stout::util {

// Windows FILETIME: 100-nanosecond intervals since 1601-01-01 00:00:00 UTC
// Unix epoch:       1970-01-01 00:00:00 UTC
// Difference:       11644473600 seconds = 116444736000000000 * 100ns

inline constexpr int64_t filetime_unix_epoch_diff = 116444736000000000LL;

// Convert a 64-bit FILETIME value to system_clock::time_point
[[nodiscard]] STOUT_API auto filetime_to_timepoint(uint64_t ft) noexcept -> file_time;

// Convert system_clock::time_point to 64-bit FILETIME value
[[nodiscard]] STOUT_API auto timepoint_to_filetime(file_time tp) noexcept -> uint64_t;

// Check if a FILETIME value represents "not set" (all zeros)
[[nodiscard]] constexpr bool filetime_is_zero(uint64_t ft) noexcept {
    return ft == 0;
}

} // namespace stout::util
