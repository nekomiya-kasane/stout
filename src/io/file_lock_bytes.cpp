#include "stout/io/file_lock_bytes.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#if defined(_MSC_VER)
#    include <io.h>
#endif

namespace stout::io {

    // ── impl helpers ─────────────────────────────────────────────────────

    static auto raw_seek(std::FILE *f, int64_t offset) -> bool {
#if defined(_MSC_VER)
        return _fseeki64(f, offset, SEEK_SET) == 0;
#else
        return std::fseek(f, static_cast<long>(offset), SEEK_SET) == 0;
#endif
    }

    static auto raw_tell_end(std::FILE *f) -> int64_t {
#if defined(_MSC_VER)
        auto cur = _ftelli64(f);
        _fseeki64(f, 0, SEEK_END);
        auto end = _ftelli64(f);
        _fseeki64(f, cur, SEEK_SET);
        return end;
#else
        auto cur = std::ftell(f);
        std::fseek(f, 0, SEEK_END);
        auto end = std::ftell(f);
        std::fseek(f, cur, SEEK_SET);
        return end;
#endif
    }

    // Load a page from disk (or zero-fill if beyond file end)
    auto file_lock_bytes::impl::ensure_page(uint64_t page_id) -> std::expected<page *, error> {
        // Fast path: last accessed page
        if (page_id == last_page_id && last_page_ptr) {
            return last_page_ptr;
        }

        auto it = cache.find(page_id);
        if (it != cache.end()) {
            last_page_id = page_id;
            last_page_ptr = &it->second;
            return last_page_ptr;
        }

        page pg;
        pg.data.resize(page_size, 0);
        pg.dirty = false;

        uint64_t file_off = page_id * page_size;
        if (file && raw_seek(file, static_cast<int64_t>(file_off))) {
            std::fread(pg.data.data(), 1, page_size, file);
            // Short reads are fine — rest stays zero
        }

        auto [ins, _] = cache.emplace(page_id, std::move(pg));
        last_page_id = page_id;
        last_page_ptr = &ins->second;
        return last_page_ptr;
    }

    // Write all dirty pages to disk (sorted + coalesced for minimal seeks)
    auto file_lock_bytes::impl::flush_dirty() -> std::expected<void, error> {
        if (!file) {
            return std::unexpected(error::io_error);
        }

        // Collect dirty page IDs and sort for sequential I/O
        std::vector<uint64_t> dirty_ids;
        for (auto &[page_id, pg] : cache) {
            if (pg.dirty) {
                dirty_ids.push_back(page_id);
            }
        }
        if (dirty_ids.empty()) {
            return {};
        }
        std::sort(dirty_ids.begin(), dirty_ids.end());

        // Write pages, coalescing adjacent ones into single fwrite calls
        size_t i = 0;
        while (i < dirty_ids.size()) {
            uint64_t start_id = dirty_ids[i];
            uint64_t end_id = start_id;

            // Find run of consecutive pages
            while (i + 1 < dirty_ids.size() && dirty_ids[i + 1] == end_id + 1) {
                ++end_id;
                ++i;
            }
            ++i;

            uint64_t file_off = start_id * page_size;
            uint64_t run_end = (end_id + 1) * page_size;
            if (run_end > logical_size) {
                run_end = logical_size;
            }
            if (file_off >= run_end) {
                continue;
            }

            if (!raw_seek(file, static_cast<int64_t>(file_off))) {
                return std::unexpected(error::io_error);
            }

            // Write each page in the run sequentially (already seeked to start)
            for (uint64_t pid = start_id; pid <= end_id; ++pid) {
                auto &pg = cache[pid];
                uint64_t pg_off = pid * page_size;
                uint64_t pg_end = pg_off + page_size;
                if (pg_end > logical_size) {
                    pg_end = logical_size;
                }
                size_t to_write = (pg_end > pg_off) ? static_cast<size_t>(pg_end - pg_off) : 0;
                if (to_write > 0) {
                    auto written = std::fwrite(pg.data.data(), 1, to_write, file);
                    if (written != to_write) {
                        return std::unexpected(error::io_error);
                    }
                }
                pg.dirty = false;
            }
        }
        if (std::fflush(file) != 0) {
            return std::unexpected(error::io_error);
        }
        return {};
    }

    file_lock_bytes::impl::~impl() {
        if (file) {
            flush_dirty(); // best-effort flush on destruction
            std::fclose(file);
        }
    }

    // ── file_lock_bytes ──────────────────────────────────────────────────

    file_lock_bytes::~file_lock_bytes() = default;

    file_lock_bytes::file_lock_bytes(file_lock_bytes &&other) noexcept : impl_(std::move(other.impl_)) {}

    file_lock_bytes &file_lock_bytes::operator=(file_lock_bytes &&other) noexcept {
        if (this != &other) {
            impl_ = std::move(other.impl_);
        }
        return *this;
    }

    auto file_lock_bytes::open(const std::filesystem::path &path, open_mode mode)
        -> std::expected<file_lock_bytes, error> {

        const wchar_t *wmode = nullptr;
        switch (mode) {
        case open_mode::read:
            wmode = L"rb";
            break;
        case open_mode::write:
            wmode = L"r+b";
            break;
        case open_mode::read_write:
            wmode = L"r+b";
            break;
        }

        file_lock_bytes lb;
        lb.impl_ = std::make_unique<impl>();
        lb.impl_->mode = mode;

        bool created_new = false;
#if defined(_MSC_VER)
        auto err = _wfopen_s(&lb.impl_->file, path.c_str(), wmode);
        if (err != 0 || !lb.impl_->file) {
            if (mode != open_mode::read) {
                err = _wfopen_s(&lb.impl_->file, path.c_str(), L"w+b");
                if (err != 0 || !lb.impl_->file) {
                    return std::unexpected(error::io_error);
                }
                created_new = true;
            } else {
                return std::unexpected(error::io_error);
            }
        }
#else
        auto narrow = path.string();
        const char *cmode = nullptr;
        switch (mode) {
        case open_mode::read:
            cmode = "rb";
            break;
        case open_mode::write:
            cmode = "r+b";
            break;
        case open_mode::read_write:
            cmode = "r+b";
            break;
        }
        lb.impl_->file = std::fopen(narrow.c_str(), cmode);
        if (!lb.impl_->file && mode != open_mode::read) {
            lb.impl_->file = std::fopen(narrow.c_str(), "w+b");
            created_new = true;
        }
        if (!lb.impl_->file) {
            return std::unexpected(error::io_error);
        }
#endif

        // Disable CRT buffering — we do our own caching
        std::setvbuf(lb.impl_->file, nullptr, _IONBF, 0);

        // Record initial file size (skip seek for newly created files)
        lb.impl_->logical_size = created_new ? 0 : static_cast<uint64_t>(raw_tell_end(lb.impl_->file));

        return lb;
    }

    auto file_lock_bytes::read_at(uint64_t offset, std::span<uint8_t> buf) -> std::expected<size_t, error> {
        if (!impl_) {
            return std::unexpected(error::io_error);
        }

        size_t total = buf.size();
        size_t done = 0;

        while (done < total) {
            uint64_t cur_off = offset + done;
            if (cur_off >= impl_->logical_size) {
                break;
            }

            uint64_t page_id = cur_off >> impl::page_bits;
            uint32_t off_in_page = static_cast<uint32_t>(cur_off & (impl::page_size - 1));

            auto pg = impl_->ensure_page(page_id);
            if (!pg) {
                return std::unexpected(pg.error());
            }

            uint32_t avail = impl::page_size - off_in_page;
            size_t remaining = std::min(static_cast<size_t>(avail), total - done);
            // Don't read past logical_size
            uint64_t page_logical_end = impl_->logical_size - page_id * impl::page_size;
            if (off_in_page >= page_logical_end) {
                break;
            }
            remaining = std::min(remaining, static_cast<size_t>(page_logical_end - off_in_page));

            std::copy_n((*pg)->data.data() + off_in_page, remaining, buf.data() + done);
            done += remaining;
        }
        return done;
    }

    auto file_lock_bytes::write_at(uint64_t offset, std::span<const uint8_t> buf) -> std::expected<size_t, error> {
        if (!impl_) {
            return std::unexpected(error::io_error);
        }

        size_t total = buf.size();
        size_t done = 0;

        while (done < total) {
            uint64_t cur_off = offset + done;
            uint64_t page_id = cur_off >> impl::page_bits;
            uint32_t off_in_page = static_cast<uint32_t>(cur_off & (impl::page_size - 1));

            auto pg = impl_->ensure_page(page_id);
            if (!pg) {
                return std::unexpected(pg.error());
            }

            uint32_t avail = impl::page_size - off_in_page;
            size_t to_copy = std::min(static_cast<size_t>(avail), total - done);

            std::copy_n(buf.data() + done, to_copy, (*pg)->data.data() + off_in_page);
            (*pg)->dirty = true;
            done += to_copy;
        }

        uint64_t end = offset + total;
        if (end > impl_->logical_size) {
            impl_->logical_size = end;
        }

        return total;
    }

    auto file_lock_bytes::flush() -> std::expected<void, error> {
        if (!impl_ || !impl_->file) {
            return std::unexpected(error::io_error);
        }
        return impl_->flush_dirty();
    }

    auto file_lock_bytes::set_size(uint64_t new_size) -> std::expected<void, error> {
        if (!impl_ || !impl_->file) {
            return std::unexpected(error::io_error);
        }

        // Flush dirty pages first
        auto r = impl_->flush_dirty();
        if (!r) {
            return r;
        }

        // Evict cached pages beyond new_size
        uint64_t max_page = (new_size + impl::page_size - 1) >> impl::page_bits;
        std::erase_if(impl_->cache, [max_page](const auto &kv) { return kv.first >= max_page; });
        // Invalidate fast-path pointer (map may have rehashed)
        impl_->last_page_id = UINT64_MAX;
        impl_->last_page_ptr = nullptr;

#if defined(_MSC_VER)
        auto fd = _fileno(impl_->file);
        if (_chsize_s(fd, static_cast<int64_t>(new_size)) != 0) {
            return std::unexpected(error::io_error);
        }
#else
        auto fd = fileno(impl_->file);
        if (ftruncate(fd, static_cast<off_t>(new_size)) != 0) {
            return std::unexpected(error::io_error);
        }
#endif

        impl_->logical_size = new_size;
        return {};
    }

    auto file_lock_bytes::size() const -> std::expected<uint64_t, error> {
        if (!impl_) {
            return std::unexpected(error::io_error);
        }
        return impl_->logical_size;
    }

} // namespace stout::io
