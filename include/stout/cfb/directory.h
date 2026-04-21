#pragma once

#include "stout/cfb/constants.h"
#include "stout/cfb/fat.h"
#include "stout/cfb/sector_io.h"
#include "stout/exports.h"
#include "stout/io/lock_bytes.h"
#include "stout/types.h"
#include "stout/util/endian.h"
#include "stout/util/filetime.h"
#include "stout/util/guid.h"
#include "stout/util/unicode.h"

#include <array>
#include <cstdint>
#include <expected>
#include <functional>
#include <string>
#include <vector>

namespace stout::cfb {

    // Node color in the red-black tree
    enum class node_color : uint8_t { red = 0x00, black = 0x01 };

    // A single 128-byte directory entry (in-memory representation)
    struct dir_entry {
        std::u16string name; // UTF-16 name (without null)
        entry_type type = entry_type::unknown;
        node_color color = node_color::red;
        uint32_t left_sibling = nostream;
        uint32_t right_sibling = nostream;
        uint32_t child = nostream;
        guid clsid = {};
        uint32_t state_bits = 0;
        uint64_t creation_time = 0; // FILETIME
        uint64_t modified_time = 0; // FILETIME
        uint32_t start_sector = endofchain;
        uint64_t stream_size = 0;

        [[nodiscard]] auto is_empty() const noexcept -> bool { return type == entry_type::unknown; }

        [[nodiscard]] auto is_storage() const noexcept -> bool {
            return type == entry_type::storage || type == entry_type::root;
        }

        [[nodiscard]] auto is_stream() const noexcept -> bool { return type == entry_type::stream; }

        [[nodiscard]] auto is_root() const noexcept -> bool { return type == entry_type::root; }

        [[nodiscard]] auto utf8_name() const -> std::string { return util::utf16le_to_utf8(name); }

        [[nodiscard]] auto to_stat() const -> entry_stat {
            entry_stat s;
            s.name = utf8_name();
            s.type = type;
            s.size = stream_size;
            s.clsid = clsid;
            s.creation_time = util::filetime_to_timepoint(creation_time);
            s.modified_time = util::filetime_to_timepoint(modified_time);
            s.state_bits = state_bits;
            return s;
        }
    };

    // Parse a single 128-byte directory entry from raw bytes
    [[nodiscard]] STOUT_API auto parse_dir_entry(std::span<const uint8_t, dir_entry_size> data, bool is_v3) noexcept
        -> dir_entry;

    // Serialize a single directory entry to 128 bytes
    STOUT_API void serialize_dir_entry(const dir_entry &entry, std::span<uint8_t, dir_entry_size> out,
                                       bool is_v3) noexcept;

    // In-memory directory: all entries loaded from the directory sector chain
    class STOUT_API directory {
      public:
        directory() = default;

        // Load all directory entries from disk
        template <io::lock_bytes LB>
        auto load(sector_io<LB> &sio, const fat_table &fat, uint32_t first_dir_sector, bool is_v3)
            -> std::expected<void, error> {
            entries_.clear();
            is_v3_ = is_v3;
            auto ss = sio.sector_size();
            auto entries_per = dir_entries_per_sector(ss);
            std::vector<uint8_t> buf(ss);

            for (auto sec_id : iterate_chain(fat, first_dir_sector)) {
                auto r = sio.read_sector(sec_id, buf);
                if (!r) {
                    return std::unexpected(r.error());
                }
                for (uint32_t j = 0; j < entries_per; ++j) {
                    auto span =
                        std::span<const uint8_t, dir_entry_size>(buf.data() + j * dir_entry_size, dir_entry_size);
                    entries_.push_back(parse_dir_entry(span, is_v3));
                }
            }
            return {};
        }

        // Flush all directory entries back to disk
        template <io::lock_bytes LB>
        auto flush(sector_io<LB> &sio, const fat_table &fat, uint32_t first_dir_sector) -> std::expected<void, error> {
            auto ss = sio.sector_size();
            auto entries_per = dir_entries_per_sector(ss);
            std::vector<uint8_t> buf(ss, 0);

            size_t idx = 0;
            for (auto sec_id : iterate_chain(fat, first_dir_sector)) {
                std::fill(buf.begin(), buf.end(), uint8_t{0});
                for (uint32_t j = 0; j < entries_per && idx < entries_.size(); ++j, ++idx) {
                    auto span = std::span<uint8_t, dir_entry_size>(buf.data() + j * dir_entry_size, dir_entry_size);
                    serialize_dir_entry(entries_[idx], span, is_v3_);
                }
                auto r = sio.write_sector(sec_id, buf);
                if (!r) {
                    return std::unexpected(r.error());
                }
            }
            return {};
        }

        // Access entries
        [[nodiscard]] auto entry(uint32_t id) const -> const dir_entry & { return entries_.at(id); }
        [[nodiscard]] auto entry(uint32_t id) -> dir_entry & { return entries_.at(id); }
        [[nodiscard]] auto root() const -> const dir_entry & { return entries_.at(0); }
        [[nodiscard]] auto root() -> dir_entry & { return entries_.at(0); }
        [[nodiscard]] auto count() const noexcept -> size_t { return entries_.size(); }

        // Find a child entry by name under a given parent (using red-black tree traversal)
        [[nodiscard]] auto find_child(uint32_t parent_id, std::u16string_view name) const -> uint32_t;

        // Add a new entry (returns its ID)
        auto add_entry() -> uint32_t {
            // Try to reuse an empty slot
            for (uint32_t i = 0; i < static_cast<uint32_t>(entries_.size()); ++i) {
                if (entries_[i].is_empty()) {
                    entries_[i] = dir_entry{};
                    return i;
                }
            }
            entries_.push_back(dir_entry{});
            return static_cast<uint32_t>(entries_.size() - 1);
        }

        // Insert a child entry into a storage's red-black tree
        void insert_child(uint32_t parent_id, uint32_t new_entry_id);

        // Remove a child entry from a storage's red-black tree
        void remove_child(uint32_t parent_id, uint32_t entry_id);

        // Enumerate all children of a storage (in-order traversal)
        void enumerate_children(uint32_t parent_id,
                                const std::function<void(uint32_t, const dir_entry &)> &callback) const;

        [[nodiscard]] auto entries() const noexcept -> const std::vector<dir_entry> & { return entries_; }

      private:
        // Red-black tree helpers
        void rb_insert_fixup(uint32_t parent_id, uint32_t node_id);
        void rb_rotate_left(uint32_t parent_id, uint32_t node_id);
        void rb_rotate_right(uint32_t parent_id, uint32_t node_id);
        void inorder_traverse(uint32_t node_id, const std::function<void(uint32_t, const dir_entry &)> &callback) const;

        // Find the parent of a node in the tree rooted at root_id
        [[nodiscard]] auto find_parent_in_tree(uint32_t root_id, uint32_t node_id) const -> uint32_t;

        std::vector<dir_entry> entries_;
        bool is_v3_ = false;
    };

} // namespace stout::cfb
