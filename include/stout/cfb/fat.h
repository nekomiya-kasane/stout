#pragma once

#include "stout/cfb/constants.h"
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

    // In-memory FAT table: maps sector_id -> next sector_id in chain
    class STOUT_API fat_table {
      public:
        fat_table() = default;

        // Load the entire FAT from disk given the list of FAT sector IDs
        template <io::lock_bytes LB>
        auto load(sector_io<LB> &sio, std::span<const uint32_t> fat_sector_ids) -> std::expected<void, error> {
            auto ss = sio.sector_size();
            auto entries_per = fat_entries_per_sector(ss);
            entries_.resize(fat_sector_ids.size() * entries_per, freesect);

            std::vector<uint8_t> buf(ss);
            for (size_t i = 0; i < fat_sector_ids.size(); ++i) {
                auto r = sio.read_sector(fat_sector_ids[i], buf);
                if (!r) {
                    return std::unexpected(r.error());
                }
                for (uint32_t j = 0; j < entries_per; ++j) {
                    entries_[i * entries_per + j] = util::read_u32_le(buf.data() + j * 4);
                }
            }
            return {};
        }

        // Flush the entire FAT back to disk
        template <io::lock_bytes LB>
        auto flush(sector_io<LB> &sio, std::span<const uint32_t> fat_sector_ids) -> std::expected<void, error> {
            auto ss = sio.sector_size();
            auto entries_per = fat_entries_per_sector(ss);
            std::vector<uint8_t> buf(ss);

            for (size_t i = 0; i < fat_sector_ids.size(); ++i) {
                for (uint32_t j = 0; j < entries_per; ++j) {
                    auto idx = i * entries_per + j;
                    uint32_t val = (idx < entries_.size()) ? entries_[idx] : freesect;
                    util::write_u32_le(buf.data() + j * 4, val);
                }
                auto r = sio.write_sector(fat_sector_ids[i], buf);
                if (!r) {
                    return std::unexpected(r.error());
                }
            }
            return {};
        }

        // Get the next sector in a chain
        [[nodiscard]] auto next(uint32_t sector_id) const noexcept -> uint32_t {
            if (sector_id >= entries_.size()) {
                return freesect;
            }
            return entries_[sector_id];
        }

        // Set the next sector in a chain
        void set(uint32_t sector_id, uint32_t next_id) {
            if (sector_id >= entries_.size()) {
                entries_.resize(sector_id + 1, freesect);
            }
            entries_[sector_id] = next_id;
        }

        // Allocate a free sector, returns its ID
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

        // Free a sector
        void free_sector(uint32_t sector_id) {
            if (sector_id < entries_.size()) {
                entries_[sector_id] = freesect;
            }
        }

        // Free an entire chain starting from start_sector
        void free_chain(uint32_t start_sector) {
            uint32_t cur = start_sector;
            while (cur != endofchain && cur != freesect && cur < entries_.size()) {
                uint32_t nxt = entries_[cur];
                entries_[cur] = freesect;
                cur = nxt;
            }
        }

        // Build a chain: collect all sector IDs starting from start_sector
        [[nodiscard]] auto chain(uint32_t start_sector) const -> std::vector<uint32_t> {
            std::vector<uint32_t> result;
            uint32_t cur = start_sector;
            while (cur != endofchain && cur != freesect && cur < entries_.size()) {
                result.push_back(cur);
                cur = entries_[cur];
                if (result.size() > entries_.size()) {
                    break; // cycle guard
                }
            }
            return result;
        }

        // Extend a chain by appending a newly allocated sector
        auto extend_chain(uint32_t start_sector) -> uint32_t {
            auto new_id = allocate();
            if (start_sector == endofchain || start_sector == freesect) {
                return new_id;
            }
            // Walk to end of chain
            uint32_t cur = start_sector;
            while (entries_[cur] != endofchain && entries_[cur] != freesect) {
                cur = entries_[cur];
                if (cur >= entries_.size()) {
                    break;
                }
            }
            entries_[cur] = new_id;
            return new_id;
        }

        [[nodiscard]] auto size() const noexcept -> size_t { return entries_.size(); }
        [[nodiscard]] auto entries() const noexcept -> const std::vector<uint32_t> & { return entries_; }
        [[nodiscard]] auto entries() noexcept -> std::vector<uint32_t> & { return entries_; }

        void resize(size_t count) { entries_.resize(count, freesect); }

      private:
        std::vector<uint32_t> entries_;
    };

    // Sector chain iterator for range-based for loops
    class sector_chain_iterator {
      public:
        using value_type = uint32_t;
        using difference_type = std::ptrdiff_t;

        sector_chain_iterator() = default;
        sector_chain_iterator(const fat_table &fat, uint32_t sector_id) : fat_(&fat), current_(sector_id) {}

        auto operator*() const noexcept -> uint32_t { return current_; }

        auto operator++() -> sector_chain_iterator & {
            if (fat_ && current_ != endofchain && current_ != freesect) {
                current_ = fat_->next(current_);
            } else {
                current_ = endofchain;
            }
            return *this;
        }

        auto operator++(int) -> sector_chain_iterator {
            auto tmp = *this;
            ++(*this);
            return tmp;
        }

        friend auto operator==(const sector_chain_iterator &a, const sector_chain_iterator &b) -> bool {
            return a.current_ == b.current_;
        }

        friend auto operator!=(const sector_chain_iterator &a, const sector_chain_iterator &b) -> bool {
            return !(a == b);
        }

      private:
        const fat_table *fat_ = nullptr;
        uint32_t current_ = endofchain;
    };

    // Range adapter for sector chains
    class sector_chain_range {
      public:
        sector_chain_range(const fat_table &fat, uint32_t start) : fat_(&fat), start_(start) {}

        [[nodiscard]] auto begin() const -> sector_chain_iterator { return {*fat_, start_}; }

        [[nodiscard]] auto end() const -> sector_chain_iterator { return {*fat_, endofchain}; }

      private:
        const fat_table *fat_;
        uint32_t start_;
    };

    // Convenience function
    [[nodiscard]] inline auto iterate_chain(const fat_table &fat, uint32_t start) -> sector_chain_range {
        return {fat, start};
    }

} // namespace stout::cfb
