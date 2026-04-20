#pragma once

#include "stout/util/guid.h"
#include <cstdint>
#include <string>
#include <expected>
#include <chrono>

namespace stout {

// ── Error type ─────────────────────────────────────────────────────────

enum class error : uint8_t {
    ok = 0,
    invalid_signature,
    invalid_version,
    invalid_sector_size,
    invalid_header,
    invalid_fat,
    invalid_difat,
    invalid_directory,
    invalid_mini_fat,
    invalid_stream,
    invalid_name,
    corrupt_file,
    not_found,
    already_exists,
    access_denied,
    io_error,
    out_of_space,
    invalid_argument,
    not_supported,
    transaction_failed,
    internal_error,
};

[[nodiscard]] constexpr auto error_message(error e) noexcept -> const char* {
    switch (e) {
        case error::ok:                 return "success";
        case error::invalid_signature:  return "invalid compound file signature";
        case error::invalid_version:    return "invalid compound file version";
        case error::invalid_sector_size:return "invalid sector size";
        case error::invalid_header:     return "invalid compound file header";
        case error::invalid_fat:        return "invalid FAT structure";
        case error::invalid_difat:      return "invalid DIFAT structure";
        case error::invalid_directory:  return "invalid directory entry";
        case error::invalid_mini_fat:   return "invalid mini FAT structure";
        case error::invalid_stream:     return "invalid stream";
        case error::invalid_name:       return "invalid entry name";
        case error::corrupt_file:       return "corrupt compound file";
        case error::not_found:          return "entry not found";
        case error::already_exists:     return "entry already exists";
        case error::access_denied:      return "access denied";
        case error::io_error:           return "I/O error";
        case error::out_of_space:       return "out of space";
        case error::invalid_argument:   return "invalid argument";
        case error::not_supported:      return "operation not supported";
        case error::transaction_failed: return "transaction failed";
        case error::internal_error:     return "internal error";
    }
    return "unknown error";
}

// ── Open / Create modes ────────────────────────────────────────────────

enum class open_mode : uint8_t {
    read,
    write,
    read_write,
};

enum class create_mode : uint8_t {
    create_new,
    open_existing,
    open_or_create,
};

enum class cfb_version : uint16_t {
    v3 = 3,
    v4 = 4,
};

struct open_options {
    open_mode    mode       = open_mode::read_write;
    create_mode  create     = create_mode::open_or_create;
    cfb_version  version    = cfb_version::v4;
    bool         transacted = false;
};

// ── Entry types ────────────────────────────────────────────────────────

enum class entry_type : uint8_t {
    unknown  = 0x00,
    storage  = 0x01,
    stream   = 0x02,
    root     = 0x05,
};

// ── Seek origin ────────────────────────────────────────────────────────

enum class seek_origin : uint8_t {
    begin,
    current,
    end,
};

// ── File time alias ────────────────────────────────────────────────────

using file_time = std::chrono::system_clock::time_point;

// ── Entry stat ─────────────────────────────────────────────────────────

struct entry_stat {
    std::string  name;
    entry_type   type          = entry_type::unknown;
    uint64_t     size          = 0;
    guid         clsid         = {};
    file_time    creation_time = {};
    file_time    modified_time = {};
    uint32_t     state_bits    = 0;
};

} // namespace stout
