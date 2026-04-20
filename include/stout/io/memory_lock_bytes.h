#pragma once

#include "stout/exports.h"
#include "stout/io/lock_bytes.h"
#include "stout/types.h"

#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace stout::io {

class STOUT_API memory_lock_bytes {
  public:
    memory_lock_bytes() = default;
    explicit memory_lock_bytes(std::vector<uint8_t> data) : data_(std::move(data)) {}

    auto read_at(uint64_t offset, std::span<uint8_t> buf) -> std::expected<size_t, error>;
    auto write_at(uint64_t offset, std::span<const uint8_t> buf) -> std::expected<size_t, error>;
    auto flush() -> std::expected<void, error>;
    auto set_size(uint64_t new_size) -> std::expected<void, error>;
    [[nodiscard]] auto size() const -> std::expected<uint64_t, error>;

    // Direct access to underlying buffer
    [[nodiscard]] auto data() const noexcept -> const std::vector<uint8_t> & { return data_; }
    [[nodiscard]] auto data() noexcept -> std::vector<uint8_t> & { return data_; }

  private:
    std::vector<uint8_t> data_;
};

static_assert(lock_bytes<memory_lock_bytes>);

} // namespace stout::io
