#include "stout/io/memory_lock_bytes.h"

#include <algorithm>
#include <cstring>

namespace stout::io {

auto memory_lock_bytes::read_at(uint64_t offset, std::span<uint8_t> buf) -> std::expected<size_t, error> {
    if (offset >= data_.size()) return size_t{0};
    auto available = data_.size() - static_cast<size_t>(offset);
    auto to_read = std::min(buf.size(), available);
    std::memcpy(buf.data(), data_.data() + offset, to_read);
    return to_read;
}

auto memory_lock_bytes::write_at(uint64_t offset, std::span<const uint8_t> buf) -> std::expected<size_t, error> {
    auto end = static_cast<size_t>(offset) + buf.size();
    if (end > data_.size()) {
        data_.resize(end, 0);
    }
    std::memcpy(data_.data() + offset, buf.data(), buf.size());
    return buf.size();
}

auto memory_lock_bytes::flush() -> std::expected<void, error> {
    return {};
}

auto memory_lock_bytes::set_size(uint64_t new_size) -> std::expected<void, error> {
    data_.resize(static_cast<size_t>(new_size), 0);
    return {};
}

auto memory_lock_bytes::size() const -> std::expected<uint64_t, error> {
    return static_cast<uint64_t>(data_.size());
}

} // namespace stout::io
