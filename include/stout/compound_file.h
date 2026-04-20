#pragma once

#include "stout/cfb/difat.h"
#include "stout/cfb/directory.h"
#include "stout/cfb/fat.h"
#include "stout/cfb/header.h"
#include "stout/cfb/mini_fat.h"
#include "stout/cfb/sector_io.h"
#include "stout/exports.h"
#include "stout/io/file_lock_bytes.h"
#include "stout/io/memory_lock_bytes.h"
#include "stout/types.h"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace stout {

class storage;
class stream;

// The main compound file class. Owns the I/O backend and all internal structures.
class STOUT_API compound_file {
  public:
    ~compound_file();
    compound_file(compound_file &&) noexcept;
    compound_file &operator=(compound_file &&) noexcept;
    compound_file(const compound_file &) = delete;
    compound_file &operator=(const compound_file &) = delete;

    // Open an existing compound file from disk
    [[nodiscard]] static auto open(const std::filesystem::path &path, open_mode mode = open_mode::read)
        -> std::expected<compound_file, error>;

    // Create a new compound file on disk
    [[nodiscard]] static auto create(const std::filesystem::path &path, cfb_version version = cfb_version::v4)
        -> std::expected<compound_file, error>;

    // Create a new in-memory compound file
    [[nodiscard]] static auto create_in_memory(cfb_version version = cfb_version::v4)
        -> std::expected<compound_file, error>;

    // Open an existing compound file from a memory buffer
    [[nodiscard]] static auto open_from_memory(std::vector<uint8_t> data) -> std::expected<compound_file, error>;

    // Get the root storage
    [[nodiscard]] auto root_storage() -> storage;

    // Flush all changes to disk
    auto flush() -> std::expected<void, error>;

    // Get the CFB version
    [[nodiscard]] auto version() const noexcept -> cfb_version;

    // Get the raw bytes (only for in-memory files)
    [[nodiscard]] auto data() const -> const std::vector<uint8_t> *;

    // Transaction support
    auto begin_transaction() -> std::expected<void, error>;
    auto commit() -> std::expected<void, error>;
    auto revert() -> std::expected<void, error>;
    [[nodiscard]] auto in_transaction() const noexcept -> bool;

    // Internal access (for storage/stream)
    struct internal;
    [[nodiscard]] auto internals() -> internal &;

  private:
    compound_file();
    struct impl;
    std::unique_ptr<impl> impl_;
};

// A storage (folder) within a compound file
class STOUT_API storage {
  public:
    storage(compound_file::internal &cf, uint32_t dir_id);

    // Create or open a child stream
    [[nodiscard]] auto create_stream(std::string_view name) -> std::expected<stream, error>;
    [[nodiscard]] auto open_stream(std::string_view name) -> std::expected<stream, error>;

    // Create or open a child storage
    [[nodiscard]] auto create_storage(std::string_view name) -> std::expected<storage, error>;
    [[nodiscard]] auto open_storage(std::string_view name) -> std::expected<storage, error>;

    // Delete a child entry
    auto remove(std::string_view name) -> std::expected<void, error>;

    // Check if a child exists
    [[nodiscard]] auto exists(std::string_view name) const -> bool;

    // Enumerate children
    [[nodiscard]] auto children() const -> std::vector<entry_stat>;

    // Get stat for this storage
    [[nodiscard]] auto stat() const -> entry_stat;

    // Get the name
    [[nodiscard]] auto name() const -> std::string;

    // Rename this entry
    auto rename(std::string_view new_name) -> std::expected<void, error>;

    // CLSID
    [[nodiscard]] auto clsid() const -> guid;
    auto set_clsid(const guid &id) -> void;

    // State bits
    [[nodiscard]] auto state_bits() const -> uint32_t;
    auto set_state_bits(uint32_t bits) -> void;
    auto set_state_bits(uint32_t bits, uint32_t mask) -> void;

    // Timestamps
    [[nodiscard]] auto creation_time() const -> file_time;
    [[nodiscard]] auto modified_time() const -> file_time;
    auto set_creation_time(file_time t) -> void;
    auto set_modified_time(file_time t) -> void;

    // Set timestamps on a child entry by name
    auto set_element_times(std::string_view name, file_time ctime, file_time mtime) -> std::expected<void, error>;

    // Copy a child entry to another storage
    auto copy_to(storage &dest, std::string_view name) -> std::expected<void, error>;

    [[nodiscard]] auto dir_id() const noexcept -> uint32_t { return dir_id_; }

  private:
    compound_file::internal *cf_;
    uint32_t dir_id_;
};

// A stream (data) within a compound file
class STOUT_API stream {
  public:
    stream(compound_file::internal &cf, uint32_t dir_id);

    // Read data from the stream
    auto read(uint64_t offset, std::span<uint8_t> buf) -> std::expected<size_t, error>;

    // Write data to the stream
    auto write(uint64_t offset, std::span<const uint8_t> buf) -> std::expected<size_t, error>;

    // Get the stream size
    [[nodiscard]] auto size() const -> uint64_t;

    // Resize the stream
    auto resize(uint64_t new_size) -> std::expected<void, error>;

    // Get stat
    [[nodiscard]] auto stat() const -> entry_stat;

    // Get the name
    [[nodiscard]] auto name() const -> std::string;

    // Rename this stream
    auto rename(std::string_view new_name) -> std::expected<void, error>;

    // Copy data to another stream
    auto copy_to(stream &dest, uint64_t bytes) -> std::expected<uint64_t, error>;

    [[nodiscard]] auto dir_id() const noexcept -> uint32_t { return dir_id_; }

  private:
    compound_file::internal *cf_;
    uint32_t dir_id_;
};

} // namespace stout
