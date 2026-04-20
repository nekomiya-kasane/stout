#pragma once

#include "stout/exports.h"
#include "stout/types.h"
#include "stout/util/guid.h"
#include "stout/util/endian.h"
#include <cstdint>
#include <expected>
#include <map>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace stout::ole {

// ── OLE Property Types (VT_*) ─────────────────────────────────────────

enum class vt : uint16_t {
    empty       = 0x0000,
    null_       = 0x0001,
    i2          = 0x0002,  // int16_t
    i4          = 0x0003,  // int32_t
    r4          = 0x0004,  // float
    r8          = 0x0005,  // double
    cy          = 0x0006,  // int64_t (currency, scaled by 10000)
    date        = 0x0007,  // double (OLE date)
    bstr        = 0x0008,  // string (code page)
    bool_       = 0x000B,  // int16_t (-1 = true, 0 = false)
    ui2         = 0x0012,  // uint16_t
    ui4         = 0x0013,  // uint32_t
    i8          = 0x0014,  // int64_t
    ui8         = 0x0015,  // uint64_t
    lpstr       = 0x001E,  // code page string
    lpwstr      = 0x001F,  // UTF-16LE string
    filetime    = 0x0040,  // uint64_t (FILETIME)
    blob        = 0x0041,  // binary blob
    clsid       = 0x0048,  // GUID
};

// ── Property value variant ─────────────────────────────────────────────

using property_value = std::variant<
    std::monostate,     // VT_EMPTY / VT_NULL
    int16_t,            // VT_I2, VT_BOOL
    int32_t,            // VT_I4
    uint16_t,           // VT_UI2
    uint32_t,           // VT_UI4
    int64_t,            // VT_I8, VT_CY
    uint64_t,           // VT_UI8, VT_FILETIME
    float,              // VT_R4
    double,             // VT_R8, VT_DATE
    std::string,        // VT_LPSTR, VT_BSTR
    std::u16string,     // VT_LPWSTR
    std::vector<uint8_t>, // VT_BLOB
    guid                // VT_CLSID
>;

// ── Property ───────────────────────────────────────────────────────────

struct property {
    uint32_t id = 0;
    vt type = vt::empty;
    property_value value;
};

// ── Well-known property IDs (SummaryInformation) ───────────────────────

namespace pid {
    inline constexpr uint32_t dictionary     = 0x00000000;
    inline constexpr uint32_t codepage       = 0x00000001;
    inline constexpr uint32_t title          = 0x00000002;
    inline constexpr uint32_t subject        = 0x00000003;
    inline constexpr uint32_t author         = 0x00000004;
    inline constexpr uint32_t keywords       = 0x00000005;
    inline constexpr uint32_t comments       = 0x00000006;
    inline constexpr uint32_t template_      = 0x00000007;
    inline constexpr uint32_t last_author    = 0x00000008;
    inline constexpr uint32_t revision       = 0x00000009;
    inline constexpr uint32_t edit_time      = 0x0000000A;
    inline constexpr uint32_t last_printed   = 0x0000000B;
    inline constexpr uint32_t create_dtm     = 0x0000000C;
    inline constexpr uint32_t last_save_dtm  = 0x0000000D;
    inline constexpr uint32_t page_count     = 0x0000000E;
    inline constexpr uint32_t word_count     = 0x0000000F;
    inline constexpr uint32_t char_count     = 0x00000010;
    inline constexpr uint32_t app_name       = 0x00000012;
    inline constexpr uint32_t security       = 0x00000013;
} // namespace pid

// ── Well-known FMTIDs ──────────────────────────────────────────────────

STOUT_API auto fmtid_summary_information() noexcept -> const guid&;
STOUT_API auto fmtid_doc_summary_information() noexcept -> const guid&;

// ── Property section ───────────────────────────────────────────────────

struct STOUT_API property_section {
    guid fmtid;
    uint16_t codepage = 1252; // default Windows-1252
    std::map<uint32_t, property> properties;

    [[nodiscard]] auto get(uint32_t id) const -> const property*;
    void set(uint32_t id, vt type, property_value val);
    void remove(uint32_t id);

    // Convenience getters
    [[nodiscard]] auto get_string(uint32_t id) const -> std::string;
    [[nodiscard]] auto get_i4(uint32_t id) const -> int32_t;
    [[nodiscard]] auto get_u4(uint32_t id) const -> uint32_t;
    [[nodiscard]] auto get_filetime(uint32_t id) const -> uint64_t;
    [[nodiscard]] auto get_bool(uint32_t id) const -> bool;

    // Convenience setters
    void set_string(uint32_t id, std::string val);
    void set_i4(uint32_t id, int32_t val);
    void set_u4(uint32_t id, uint32_t val);
    void set_filetime(uint32_t id, uint64_t val);
    void set_bool(uint32_t id, bool val);
};

// ── Property set (one or two sections) ─────────────────────────────────

struct STOUT_API property_set {
    uint16_t byte_order = 0xFFFE;
    uint16_t format_version = 0;
    uint32_t os_version = 0;
    guid clsid;
    std::vector<property_section> sections;

    [[nodiscard]] auto section(const guid& fmtid) -> property_section*;
    [[nodiscard]] auto section(const guid& fmtid) const -> const property_section*;
    auto add_section(const guid& fmtid) -> property_section&;
};

// ── Parse / Serialize ──────────────────────────────────────────────────

[[nodiscard]] STOUT_API auto parse_property_set(std::span<const uint8_t> data)
    -> std::expected<property_set, error>;

[[nodiscard]] STOUT_API auto serialize_property_set(const property_set& ps)
    -> std::expected<std::vector<uint8_t>, error>;

} // namespace stout::ole
