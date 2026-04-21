/**
 * @file paged_reader.h
 * @brief On-demand paged stream reader — loads 4 KB pages as needed, no size cap.
 *
 * Replaces the old approach of loading the entire stream (capped at 64 KB) into
 * a single vector. Pages are cached in an LRU-friendly map so repeated access
 * to the same region is free.
 */
#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <span>
#include <unordered_map>
#include <vector>

namespace ssv {

    /// @brief On-demand paged reader for stream data.
    ///
    /// Instead of loading the entire stream into memory, this reader fetches
    /// fixed-size pages on demand via a user-supplied callback. Pages are cached
    /// so that scrolling through the hex view only reads each page once.
    class paged_reader {
      public:
        /// @brief Page size in bytes (4 KB).
        static constexpr uint32_t page_size = 4096;

        /// @brief Callback type: read up to `len` bytes at `offset` into `buf`.
        ///        Returns the number of bytes actually read.
        using read_fn = std::function<size_t(uint64_t offset, std::span<uint8_t> buf)>;

        paged_reader() = default;

        /// @brief Construct a paged reader for a stream of known size.
        /// @param total_size  Total stream size in bytes.
        /// @param reader      Callback that reads bytes from the stream.
        paged_reader(uint64_t total_size, read_fn reader) : total_size_(total_size), reader_(std::move(reader)) {}

        /// @brief Total stream size in bytes.
        [[nodiscard]] uint64_t total_size() const noexcept { return total_size_; }

        /// @brief Total number of hex lines (16 bytes per line).
        [[nodiscard]] uint32_t total_lines() const noexcept { return static_cast<uint32_t>((total_size_ + 15) / 16); }

        /// @brief Whether the reader has been initialized with a stream.
        [[nodiscard]] bool valid() const noexcept { return reader_ != nullptr; }

        /// @brief Read a single byte at the given offset. Returns 0 if out of range.
        [[nodiscard]] uint8_t byte_at(uint64_t offset) {
            if (offset >= total_size_) {
                return 0;
            }
            auto &page = ensure_page(offset / page_size);
            uint32_t off_in_page = static_cast<uint32_t>(offset % page_size);
            if (off_in_page >= page.size()) {
                return 0;
            }
            return page[off_in_page];
        }

        /// @brief Read a contiguous range of bytes. Returns the number of bytes copied.
        size_t read_range(uint64_t offset, std::span<uint8_t> out) {
            size_t copied = 0;
            while (copied < out.size() && offset + copied < total_size_) {
                uint64_t abs_off = offset + copied;
                uint64_t page_idx = abs_off / page_size;
                uint32_t off_in_page = static_cast<uint32_t>(abs_off % page_size);
                auto &page = ensure_page(page_idx);
                uint32_t avail = static_cast<uint32_t>(page.size()) - off_in_page;
                uint32_t to_copy = static_cast<uint32_t>(std::min<size_t>(avail, out.size() - copied));
                std::copy_n(page.data() + off_in_page, to_copy, out.data() + copied);
                copied += to_copy;
            }
            return copied;
        }

        /// @brief Read 16 bytes for a hex line at the given line index.
        ///        Returns the actual number of bytes available (0–16).
        uint32_t read_hex_line(uint32_t line_idx, uint8_t out[16]) {
            uint64_t offset = static_cast<uint64_t>(line_idx) * 16;
            if (offset >= total_size_) {
                return 0;
            }
            uint32_t avail = static_cast<uint32_t>(std::min<uint64_t>(16, total_size_ - offset));
            std::span<uint8_t> buf(out, avail);
            read_range(offset, buf);
            return avail;
        }

        /// @brief Drop all cached pages (e.g. when switching to a different stream).
        void clear() {
            pages_.clear();
            total_size_ = 0;
            reader_ = nullptr;
        }

        /// @brief Number of pages currently cached.
        [[nodiscard]] size_t cached_pages() const noexcept { return pages_.size(); }

      private:
        uint64_t total_size_ = 0;
        read_fn reader_;
        std::unordered_map<uint64_t, std::vector<uint8_t>> pages_;

        /// @brief Ensure a page is loaded and return a reference to its data.
        std::vector<uint8_t> &ensure_page(uint64_t page_idx) {
            auto it = pages_.find(page_idx);
            if (it != pages_.end()) {
                return it->second;
            }

            uint64_t offset = page_idx * page_size;
            uint64_t remaining = (offset < total_size_) ? total_size_ - offset : 0;
            uint32_t to_read = static_cast<uint32_t>(std::min<uint64_t>(page_size, remaining));

            std::vector<uint8_t> buf(to_read);
            if (to_read > 0 && reader_) {
                size_t got = reader_(offset, std::span<uint8_t>(buf));
                buf.resize(got);
            }
            auto [ins, _] = pages_.emplace(page_idx, std::move(buf));
            return ins->second;
        }
    };

} // namespace ssv
