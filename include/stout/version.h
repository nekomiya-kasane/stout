#pragma once

#include "stout/exports.h"

#include <cstdint>

namespace stout {

struct version_info {
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
};

[[nodiscard]] STOUT_API auto library_version() noexcept -> version_info;
[[nodiscard]] STOUT_API auto library_version_string() noexcept -> const char *;

} // namespace stout
