#pragma once

#include "stout/exports.h"
#include "stout/types.h"
#include "stout/io/lock_bytes.h"
#include <cstdint>
#include <cstdio>
#include <expected>
#include <filesystem>
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>

namespace stout::io {

class STOUT_API file_lock_bytes {
public:
    ~file_lock_bytes();
    file_lock_bytes(file_lock_bytes&& other) noexcept;
    file_lock_bytes& operator=(file_lock_bytes&& other) noexcept;

    file_lock_bytes(const file_lock_bytes&) = delete;
    file_lock_bytes& operator=(const file_lock_bytes&) = delete;

    static auto open(const std::filesystem::path& path, open_mode mode)
        -> std::expected<file_lock_bytes, error>;

    auto read_at(uint64_t offset, std::span<uint8_t> buf) -> std::expected<size_t, error>;
    auto write_at(uint64_t offset, std::span<const uint8_t> buf) -> std::expected<size_t, error>;
    auto flush() -> std::expected<void, error>;
    auto set_size(uint64_t new_size) -> std::expected<void, error>;
    [[nodiscard]] auto size() const -> std::expected<uint64_t, error>;

    struct impl {
        std::FILE* file = nullptr;
        open_mode mode = open_mode::read;

        // Write-back page cache (4 KB pages)
        static constexpr uint32_t page_bits = 12;
        static constexpr uint32_t page_size = 1u << page_bits; // 4096
        struct page {
            std::vector<uint8_t> data;
            bool dirty = false;
        };
        std::unordered_map<uint64_t, page> cache;
        uint64_t logical_size = 0; // tracks size including cached writes

        // Fast path: last accessed page avoids hash lookup
        uint64_t last_page_id = UINT64_MAX;
        page* last_page_ptr = nullptr;

        auto flush_dirty() -> std::expected<void, error>;
        auto ensure_page(uint64_t page_id) -> std::expected<page*, error>;

        ~impl();
    };

private:
    file_lock_bytes() = default;
    std::unique_ptr<impl> impl_;
};

static_assert(lock_bytes<file_lock_bytes>);

} // namespace stout::io
