#pragma once

#include "stout/types.h"
#include <cstdint>
#include <expected>
#include <span>

namespace stout::io {

// Concept for a physical I/O backend (analogous to ILockBytes)
template<typename T>
concept lock_bytes = requires(T& lb, const T& clb,
                              uint64_t offset,
                              std::span<uint8_t> buf,
                              std::span<const uint8_t> cbuf) {
    { lb.read_at(offset, buf) }   -> std::same_as<std::expected<size_t, error>>;
    { lb.write_at(offset, cbuf) } -> std::same_as<std::expected<size_t, error>>;
    { lb.flush() }                -> std::same_as<std::expected<void, error>>;
    { lb.set_size(uint64_t{}) }   -> std::same_as<std::expected<void, error>>;
    { clb.size() }                -> std::same_as<std::expected<uint64_t, error>>;
};

} // namespace stout::io
