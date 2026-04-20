#pragma once

#include "stout/exports.h"
#include "stout/types.h"
#include "stout/io/lock_bytes.h"
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace stout::cfb {

// Convert sector number to byte offset in the file
[[nodiscard]] constexpr auto sector_offset(uint32_t sector_id, uint32_t sector_size) noexcept -> uint64_t {
    return static_cast<uint64_t>(sector_id + 1) * sector_size;
}

// Sector-level I/O operations parameterized on a lock_bytes backend
template<io::lock_bytes LB>
class sector_io {
public:
    explicit sector_io(LB& backend, uint32_t sector_size) noexcept
        : backend_(&backend), sector_size_(sector_size) {}

    [[nodiscard]] auto sector_size() const noexcept -> uint32_t { return sector_size_; }

    auto read_sector(uint32_t sector_id, std::span<uint8_t> buf) -> std::expected<void, error> {
        if (buf.size() < sector_size_) return std::unexpected(error::invalid_argument);
        auto offset = sector_offset(sector_id, sector_size_);
        auto result = backend_->read_at(offset, buf.subspan(0, sector_size_));
        if (!result) return std::unexpected(result.error());
        if (*result != sector_size_) return std::unexpected(error::io_error);
        return {};
    }

    auto write_sector(uint32_t sector_id, std::span<const uint8_t> buf) -> std::expected<void, error> {
        if (buf.size() < sector_size_) return std::unexpected(error::invalid_argument);
        auto offset = sector_offset(sector_id, sector_size_);
        auto result = backend_->write_at(offset, buf.subspan(0, sector_size_));
        if (!result) return std::unexpected(result.error());
        if (*result != sector_size_) return std::unexpected(error::io_error);
        return {};
    }

    auto read_header(std::span<uint8_t, 512> buf) -> std::expected<void, error> {
        auto result = backend_->read_at(0, std::span<uint8_t>{buf.data(), 512});
        if (!result) return std::unexpected(result.error());
        if (*result != 512) return std::unexpected(error::io_error);
        return {};
    }

    auto write_header(std::span<const uint8_t, 512> buf) -> std::expected<void, error> {
        auto result = backend_->write_at(0, std::span<const uint8_t>{buf.data(), 512});
        if (!result) return std::unexpected(result.error());
        if (*result != 512) return std::unexpected(error::io_error);
        return {};
    }

    // Read a portion of a sector
    auto read_at(uint32_t sector_id, uint32_t offset_in_sector,
                 std::span<uint8_t> buf) -> std::expected<size_t, error> {
        auto offset = sector_offset(sector_id, sector_size_) + offset_in_sector;
        return backend_->read_at(offset, buf);
    }

    // Write a portion of a sector
    auto write_at(uint32_t sector_id, uint32_t offset_in_sector,
                  std::span<const uint8_t> buf) -> std::expected<size_t, error> {
        auto offset = sector_offset(sector_id, sector_size_) + offset_in_sector;
        return backend_->write_at(offset, buf);
    }

    auto flush() -> std::expected<void, error> { return backend_->flush(); }

private:
    LB* backend_;
    uint32_t sector_size_;
};

} // namespace stout::cfb
