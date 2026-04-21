#pragma once

#include "stout/cfb/constants.h"
#include "stout/cfb/fat.h"
#include "stout/cfb/sector_io.h"
#include "stout/exports.h"
#include "stout/io/lock_bytes.h"
#include "stout/types.h"
#include "stout/util/endian.h"

#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace stout::cfb {

// In-memory Mini FAT table: maps mini_sector_id -> next mini_sector_id
// Works identically to fat_table but for 64-byte mini sectors.
// The mini FAT itself is stored in regular sectors chained via the main FAT.
class STOUT_API mini_fat_table {
  public:
    mini_fat_table() = default;

    // Load the mini FAT from disk given the chain of sectors holding it
    template <io::lock_bytes LB>
    auto load(sector_io<LB> &sio, const fat_table &fat, uint32_t first_mini_fat_sector) -> std::expected<void, error> {
        entries_.clear();
        if (first_mini_fat_sector == endofchain || first_mini_fat_sector == freesect) {
            return {};
        }

        auto ss = sio.sector_size();
        auto entries_per = fat_entries_per_sector(ss); // same layout as FAT
        std::vector<uint8_t> buf(ss);

        for (auto sec_id : iterate_chain(fat, first_mini_fat_sector)) {
            auto r = sio.read_sector(sec_id, buf);
            if (!r) {
                return std::unexpected(r.error());
            }
            for (uint32_t j = 0; j < entries_per; ++j) {
                entries_.push_back(util::read_u32_le(buf.data() + j * 4));
            }
        }
        return {};
    }

    // Flush the mini FAT back to disk
    template <io::lock_bytes LB>
    auto flush(sector_io<LB> &sio, const fat_table &fat, uint32_t first_mini_fat_sector) -> std::expected<void, error> {
        if (first_mini_fat_sector == endofchain || first_mini_fat_sector == freesect) {
            return {};
        }

        auto ss = sio.sector_size();
        auto entries_per = fat_entries_per_sector(ss);
        std::vector<uint8_t> buf(ss);

        size_t idx = 0;
        for (auto sec_id : iterate_chain(fat, first_mini_fat_sector)) {
            std::fill(buf.begin(), buf.end(), uint8_t{0});
            for (uint32_t j = 0; j < entries_per; ++j) {
                uint32_t val = (idx < entries_.size()) ? entries_[idx] : freesect;
                util::write_u32_le(buf.data() + j * 4, val);
                ++idx;
            }
            auto r = sio.write_sector(sec_id, buf);
            if (!r) {
                return std::unexpected(r.error());
            }
        }
        return {};
    }

    [[nodiscard]] auto next(uint32_t mini_sector_id) const noexcept -> uint32_t {
        if (mini_sector_id >= entries_.size()) {
            return freesect;
        }
        return entries_[mini_sector_id];
    }

    void set(uint32_t mini_sector_id, uint32_t next_id) {
        if (mini_sector_id >= entries_.size()) {
            entries_.resize(mini_sector_id + 1, freesect);
        }
        entries_[mini_sector_id] = next_id;
    }

    [[nodiscard]] auto allocate() -> uint32_t {
        for (uint32_t i = 0; i < static_cast<uint32_t>(entries_.size()); ++i) {
            if (entries_[i] == freesect) {
                entries_[i] = endofchain;
                return i;
            }
        }
        auto id = static_cast<uint32_t>(entries_.size());
        entries_.push_back(endofchain);
        return id;
    }

    void free_sector(uint32_t mini_sector_id) {
        if (mini_sector_id < entries_.size()) {
            entries_[mini_sector_id] = freesect;
        }
    }

    void free_chain(uint32_t start) {
        uint32_t cur = start;
        while (cur != endofchain && cur != freesect && cur < entries_.size()) {
            uint32_t nxt = entries_[cur];
            entries_[cur] = freesect;
            cur = nxt;
        }
    }

    [[nodiscard]] auto chain(uint32_t start) const -> std::vector<uint32_t> {
        std::vector<uint32_t> result;
        uint32_t cur = start;
        while (cur != endofchain && cur != freesect && cur < entries_.size()) {
            result.push_back(cur);
            cur = entries_[cur];
            if (result.size() > entries_.size()) {
                break;
            }
        }
        return result;
    }

    [[nodiscard]] auto size() const noexcept -> size_t { return entries_.size(); }
    void resize(size_t count) { entries_.resize(count, freesect); }

  private:
    std::vector<uint32_t> entries_;
};

// Mini stream I/O: reads/writes data from the mini stream container.
// The mini stream container is the data of the root directory entry,
// stored as a chain of regular sectors. Mini sectors are 64-byte slices
// within that container.
class STOUT_API mini_stream_io {
  public:
    mini_stream_io() = default;

    // Initialize with the root entry's stream chain (regular sectors)
    // and the mini sector size (always 64).
    void init(std::vector<uint32_t> root_chain, uint32_t sector_size,
              uint32_t mini_sec_size = mini_sector_size) noexcept {
        root_chain_ = std::move(root_chain);
        sector_size_ = sector_size;
        mini_sector_size_ = mini_sec_size;
    }

    // Read data from a mini sector
    template <io::lock_bytes LB>
    auto read_mini_sector(sector_io<LB> &sio, uint32_t mini_sector_id, std::span<uint8_t> buf)
        -> std::expected<void, error> {
        if (buf.size() < mini_sector_size_) {
            return std::unexpected(error::invalid_argument);
        }
        auto [reg_sector, offset] = locate(mini_sector_id);
        if (reg_sector >= root_chain_.size()) {
            return std::unexpected(error::io_error);
        }
        auto result = sio.read_at(root_chain_[reg_sector], offset, buf.subspan(0, mini_sector_size_));
        if (!result) {
            return std::unexpected(result.error());
        }
        return {};
    }

    // Write data to a mini sector
    template <io::lock_bytes LB>
    auto write_mini_sector(sector_io<LB> &sio, uint32_t mini_sector_id, std::span<const uint8_t> buf)
        -> std::expected<void, error> {
        if (buf.size() < mini_sector_size_) {
            return std::unexpected(error::invalid_argument);
        }
        auto [reg_sector, offset] = locate(mini_sector_id);
        if (reg_sector >= root_chain_.size()) {
            return std::unexpected(error::io_error);
        }
        auto result = sio.write_at(root_chain_[reg_sector], offset, buf.subspan(0, mini_sector_size_));
        if (!result) {
            return std::unexpected(result.error());
        }
        return {};
    }

    // Read arbitrary bytes from a mini stream chain
    template <io::lock_bytes LB>
    auto read_mini_stream(sector_io<LB> &sio, const mini_fat_table &mfat, uint32_t start_mini_sector, uint64_t offset,
                          std::span<uint8_t> buf) -> std::expected<size_t, error> {
        auto chain = mfat.chain(start_mini_sector);
        if (chain.empty()) {
            return size_t{0};
        }

        size_t bytes_read = 0;
        uint32_t skip_sectors = static_cast<uint32_t>(offset / mini_sector_size_);
        uint32_t offset_in_sector = static_cast<uint32_t>(offset % mini_sector_size_);

        std::vector<uint8_t> sec_buf(mini_sector_size_);
        for (size_t i = skip_sectors; i < chain.size() && bytes_read < buf.size(); ++i) {
            auto r = read_mini_sector(sio, chain[i], sec_buf);
            if (!r) {
                return std::unexpected(r.error());
            }

            uint32_t start = (i == skip_sectors) ? offset_in_sector : 0;
            uint32_t avail = mini_sector_size_ - start;
            auto to_copy = std::min(static_cast<size_t>(avail), buf.size() - bytes_read);
            std::copy_n(sec_buf.data() + start, to_copy, buf.data() + bytes_read);
            bytes_read += to_copy;
        }
        return bytes_read;
    }

    // Write arbitrary bytes to a mini stream chain
    template <io::lock_bytes LB>
    auto write_mini_stream(sector_io<LB> &sio, const mini_fat_table &mfat, uint32_t start_mini_sector, uint64_t offset,
                           std::span<const uint8_t> buf) -> std::expected<size_t, error> {
        auto chain = mfat.chain(start_mini_sector);
        if (chain.empty()) {
            return size_t{0};
        }

        size_t bytes_written = 0;
        uint32_t skip_sectors = static_cast<uint32_t>(offset / mini_sector_size_);
        uint32_t offset_in_sector = static_cast<uint32_t>(offset % mini_sector_size_);

        std::vector<uint8_t> sec_buf(mini_sector_size_);
        for (size_t i = skip_sectors; i < chain.size() && bytes_written < buf.size(); ++i) {
            uint32_t start = (i == skip_sectors) ? offset_in_sector : 0;
            uint32_t avail = mini_sector_size_ - start;
            auto to_copy = std::min(static_cast<size_t>(avail), buf.size() - bytes_written);

            // If partial write, read first
            if (start != 0 || to_copy != mini_sector_size_) {
                auto r = read_mini_sector(sio, chain[i], sec_buf);
                if (!r) {
                    return std::unexpected(r.error());
                }
            }

            std::copy_n(buf.data() + bytes_written, to_copy, sec_buf.data() + start);
            auto w = write_mini_sector(sio, chain[i], sec_buf);
            if (!w) {
                return std::unexpected(w.error());
            }
            bytes_written += to_copy;
        }
        return bytes_written;
    }

    [[nodiscard]] auto root_chain() const noexcept -> const std::vector<uint32_t> & { return root_chain_; }

    [[nodiscard]] auto mini_sec_size() const noexcept -> uint32_t { return mini_sector_size_; }

  private:
    // Given a mini sector ID, compute which regular sector in the root chain
    // it falls in, and the byte offset within that sector.
    [[nodiscard]] auto locate(uint32_t mini_sector_id) const noexcept -> std::pair<uint32_t, uint32_t> {
        uint64_t byte_offset = static_cast<uint64_t>(mini_sector_id) * mini_sector_size_;
        auto reg_sector = static_cast<uint32_t>(byte_offset / sector_size_);
        auto offset_in_sector = static_cast<uint32_t>(byte_offset % sector_size_);
        return {reg_sector, offset_in_sector};
    }

    std::vector<uint32_t> root_chain_;
    uint32_t sector_size_ = 0;
    uint32_t mini_sector_size_ = mini_sector_size;
};

// Determine if a stream should use the mini stream (size < cutoff)
[[nodiscard]] constexpr auto use_mini_stream(uint64_t stream_size) noexcept -> bool {
    return stream_size < mini_stream_cutoff;
}

} // namespace stout::cfb
