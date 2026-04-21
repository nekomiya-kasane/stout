#pragma once

#include "stout/cfb/constants.h"
#include "stout/cfb/header.h"
#include "stout/cfb/sector_io.h"
#include "stout/exports.h"
#include "stout/io/lock_bytes.h"
#include "stout/types.h"
#include "stout/util/endian.h"

#include <cstdint>
#include <expected>
#include <vector>

namespace stout::cfb {

// Manages the DIFAT — the array of sector IDs that locate FAT sectors.
// The first 109 entries are stored in the header; additional entries
// are stored in dedicated DIFAT sectors linked via their last 4 bytes.
class STOUT_API difat_table {
  public:
    difat_table() = default;

    // Load the full DIFAT from a parsed header + any DIFAT sector chain
    template <io::lock_bytes LB> auto load(const cfb_header &hdr, sector_io<LB> &sio) -> std::expected<void, error> {

        entries_.clear();
        auto ss = sio.sector_size();

        // Collect entries from header (109 slots)
        for (uint32_t i = 0; i < difat_in_header; ++i) {
            if (hdr.difat[i] != freesect) {
                entries_.push_back(hdr.difat[i]);
            }
        }

        // Follow DIFAT sector chain
        uint32_t difat_sector = hdr.first_difat_sector;
        uint32_t remaining = hdr.total_difat_sectors;
        auto entries_per = difat_entries_per_sector(ss); // ss/4 - 1

        std::vector<uint8_t> buf(ss);
        while (difat_sector != endofchain && difat_sector != freesect && remaining > 0) {
            auto r = sio.read_sector(difat_sector, buf);
            if (!r) {
                return std::unexpected(r.error());
            }

            for (uint32_t i = 0; i < entries_per; ++i) {
                uint32_t val = util::read_u32_le(buf.data() + i * 4);
                if (val != freesect) {
                    entries_.push_back(val);
                }
            }

            // Last 4 bytes = next DIFAT sector
            difat_sector = util::read_u32_le(buf.data() + entries_per * 4);
            --remaining;
        }

        return {};
    }

    // Write the DIFAT back to the header and any DIFAT sectors
    template <io::lock_bytes LB> auto flush(cfb_header &hdr, sector_io<LB> &sio) -> std::expected<void, error> {

        auto ss = sio.sector_size();
        auto entries_per = difat_entries_per_sector(ss);

        // Fill header DIFAT slots
        hdr.difat.fill(freesect);
        size_t idx = 0;
        for (uint32_t i = 0; i < difat_in_header && idx < entries_.size(); ++i, ++idx) {
            hdr.difat[i] = entries_[idx];
        }

        // If we need DIFAT sectors
        if (idx < entries_.size()) {
            // Collect existing DIFAT sector chain
            std::vector<uint32_t> difat_sectors;
            uint32_t ds = hdr.first_difat_sector;
            while (ds != endofchain && ds != freesect) {
                difat_sectors.push_back(ds);
                // Read to get next link
                std::vector<uint8_t> buf(ss);
                auto r = sio.read_sector(ds, buf);
                if (!r) {
                    return std::unexpected(r.error());
                }
                ds = util::read_u32_le(buf.data() + entries_per * 4);
            }

            // Calculate how many DIFAT sectors we need
            size_t remaining_entries = entries_.size() - idx;
            size_t needed = (remaining_entries + entries_per - 1) / entries_per;

            // Allocate more DIFAT sectors if needed (caller must handle FAT allocation)
            while (difat_sectors.size() < needed) {
                // This is a simplified placeholder — real allocation needs FAT coordination
                difat_sectors.push_back(0); // caller must set proper sector IDs
            }

            hdr.first_difat_sector = difat_sectors.empty() ? endofchain : difat_sectors[0];
            hdr.total_difat_sectors = static_cast<uint32_t>(difat_sectors.size());

            // Write DIFAT sectors
            for (size_t s = 0; s < difat_sectors.size(); ++s) {
                std::vector<uint8_t> buf(ss, 0);
                for (uint32_t j = 0; j < entries_per && idx < entries_.size(); ++j, ++idx) {
                    util::write_u32_le(buf.data() + j * 4, entries_[idx]);
                }
                // Fill remaining with FREESECT
                for (uint32_t j = static_cast<uint32_t>(
                         std::min(entries_.size() - (idx - entries_per), static_cast<size_t>(entries_per)));
                     j < entries_per; ++j) {
                    // Already zeroed, write FREESECT
                    util::write_u32_le(buf.data() + j * 4, freesect);
                }
                // Next DIFAT sector link
                uint32_t next_ds = (s + 1 < difat_sectors.size()) ? difat_sectors[s + 1] : endofchain;
                util::write_u32_le(buf.data() + entries_per * 4, next_ds);

                auto r = sio.write_sector(difat_sectors[s], buf);
                if (!r) {
                    return std::unexpected(r.error());
                }
            }
        } else {
            hdr.first_difat_sector = endofchain;
            hdr.total_difat_sectors = 0;
        }

        return {};
    }

    [[nodiscard]] auto fat_sector_ids() const noexcept -> const std::vector<uint32_t> & { return entries_; }

    [[nodiscard]] auto fat_sector_ids() noexcept -> std::vector<uint32_t> & { return entries_; }

    void add_fat_sector(uint32_t sector_id) { entries_.push_back(sector_id); }

    [[nodiscard]] auto count() const noexcept -> size_t { return entries_.size(); }

  private:
    std::vector<uint32_t> entries_;
};

} // namespace stout::cfb
