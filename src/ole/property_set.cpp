#include "stout/ole/property_set.h"
#include <cstring>

namespace stout::ole {

// ── Well-known FMTIDs ──────────────────────────────────────────────────

static const guid s_fmtid_summary = {
    0xF29F85E0, 0x4FF9, 0x1068, {0xAB, 0x91, 0x08, 0x00, 0x2B, 0x27, 0xB3, 0xD9}
};

static const guid s_fmtid_doc_summary = {
    0xD5CDD502, 0x2E9C, 0x101B, {0x93, 0x97, 0x08, 0x00, 0x2B, 0x2C, 0xF9, 0xAE}
};

auto fmtid_summary_information() noexcept -> const guid& { return s_fmtid_summary; }
auto fmtid_doc_summary_information() noexcept -> const guid& { return s_fmtid_doc_summary; }

// ── property_section methods ───────────────────────────────────────────

auto property_section::get(uint32_t id) const -> const property* {
    auto it = properties.find(id);
    return it != properties.end() ? &it->second : nullptr;
}

void property_section::set(uint32_t id, vt type, property_value val) {
    properties[id] = property{id, type, std::move(val)};
}

void property_section::remove(uint32_t id) {
    properties.erase(id);
}

auto property_section::get_string(uint32_t id) const -> std::string {
    auto p = get(id);
    if (!p) return {};
    if (auto* s = std::get_if<std::string>(&p->value)) return *s;
    return {};
}

auto property_section::get_i4(uint32_t id) const -> int32_t {
    auto p = get(id);
    if (!p) return 0;
    if (auto* v = std::get_if<int32_t>(&p->value)) return *v;
    return 0;
}

auto property_section::get_u4(uint32_t id) const -> uint32_t {
    auto p = get(id);
    if (!p) return 0;
    if (auto* v = std::get_if<uint32_t>(&p->value)) return *v;
    return 0;
}

auto property_section::get_filetime(uint32_t id) const -> uint64_t {
    auto p = get(id);
    if (!p) return 0;
    if (auto* v = std::get_if<uint64_t>(&p->value)) return *v;
    return 0;
}

auto property_section::get_bool(uint32_t id) const -> bool {
    auto p = get(id);
    if (!p) return false;
    if (auto* v = std::get_if<int16_t>(&p->value)) return *v != 0;
    return false;
}

void property_section::set_string(uint32_t id, std::string val) {
    set(id, vt::lpstr, std::move(val));
}

void property_section::set_i4(uint32_t id, int32_t val) {
    set(id, vt::i4, val);
}

void property_section::set_u4(uint32_t id, uint32_t val) {
    set(id, vt::ui4, val);
}

void property_section::set_filetime(uint32_t id, uint64_t val) {
    set(id, vt::filetime, val);
}

void property_section::set_bool(uint32_t id, bool val) {
    set(id, vt::bool_, static_cast<int16_t>(val ? -1 : 0));
}

// ── property_set methods ───────────────────────────────────────────────

auto property_set::section(const guid& fmtid) -> property_section* {
    for (auto& s : sections) {
        if (s.fmtid == fmtid) return &s;
    }
    return nullptr;
}

auto property_set::section(const guid& fmtid) const -> const property_section* {
    for (auto& s : sections) {
        if (s.fmtid == fmtid) return &s;
    }
    return nullptr;
}

auto property_set::add_section(const guid& fmtid) -> property_section& {
    if (auto* s = section(fmtid)) return *s;
    sections.push_back({});
    sections.back().fmtid = fmtid;
    return sections.back();
}

// ── Helpers ────────────────────────────────────────────────────────────

static auto read_u16(const uint8_t* p) -> uint16_t { return util::read_u16_le(p); }
static auto read_u32(const uint8_t* p) -> uint32_t { return util::read_u32_le(p); }
static auto read_u64(const uint8_t* p) -> uint64_t { return util::read_u64_le(p); }

static void write_u16(uint8_t* p, uint16_t v) { util::write_u16_le(p, v); }
static void write_u32(uint8_t* p, uint32_t v) { util::write_u32_le(p, v); }
static void write_u64(uint8_t* p, uint64_t v) { util::write_u64_le(p, v); }

static auto pad4(uint32_t n) -> uint32_t { return (n + 3) & ~3u; }

// ── Parse a single typed value ─────────────────────────────────────────

static auto parse_typed_value(std::span<const uint8_t> data, uint32_t offset,
                              uint16_t codepage = 1252)
    -> std::expected<property, error> {
    if (offset + 4 > data.size()) return std::unexpected(error::corrupt_file);

    property prop;
    prop.type = static_cast<vt>(read_u16(data.data() + offset));
    // skip 2 padding bytes after type
    uint32_t val_off = offset + 4;

    switch (prop.type) {
    case vt::empty:
    case vt::null_:
        prop.value = std::monostate{};
        break;

    case vt::i2:
        if (val_off + 2 > data.size()) return std::unexpected(error::corrupt_file);
        prop.value = static_cast<int16_t>(read_u16(data.data() + val_off));
        break;

    case vt::i4:
        if (val_off + 4 > data.size()) return std::unexpected(error::corrupt_file);
        prop.value = static_cast<int32_t>(read_u32(data.data() + val_off));
        break;

    case vt::ui2:
        if (val_off + 2 > data.size()) return std::unexpected(error::corrupt_file);
        prop.value = read_u16(data.data() + val_off);
        break;

    case vt::ui4:
        if (val_off + 4 > data.size()) return std::unexpected(error::corrupt_file);
        prop.value = read_u32(data.data() + val_off);
        break;

    case vt::i8:
    case vt::cy:
        if (val_off + 8 > data.size()) return std::unexpected(error::corrupt_file);
        prop.value = static_cast<int64_t>(read_u64(data.data() + val_off));
        break;

    case vt::ui8:
    case vt::filetime:
        if (val_off + 8 > data.size()) return std::unexpected(error::corrupt_file);
        prop.value = read_u64(data.data() + val_off);
        break;

    case vt::r4:
        if (val_off + 4 > data.size()) return std::unexpected(error::corrupt_file);
        {
            float f;
            std::memcpy(&f, data.data() + val_off, 4);
            prop.value = f;
        }
        break;

    case vt::r8:
    case vt::date:
        if (val_off + 8 > data.size()) return std::unexpected(error::corrupt_file);
        {
            double d;
            std::memcpy(&d, data.data() + val_off, 8);
            prop.value = d;
        }
        break;

    case vt::bool_:
        if (val_off + 2 > data.size()) return std::unexpected(error::corrupt_file);
        prop.value = static_cast<int16_t>(read_u16(data.data() + val_off));
        break;

    case vt::lpstr:
    case vt::bstr:
        if (val_off + 4 > data.size()) return std::unexpected(error::corrupt_file);
        {
            uint32_t len = read_u32(data.data() + val_off);
            if (val_off + 4 + len > data.size()) return std::unexpected(error::corrupt_file);
            if (codepage == 1200 && len >= 2) {
                // Codepage 1200 = UTF-16LE: decode as u16string
                uint32_t char_count = len / 2;
                std::u16string str;
                str.reserve(char_count);
                for (uint32_t i = 0; i < char_count; ++i) {
                    auto ch = read_u16(data.data() + val_off + 4 + i * 2);
                    if (ch == 0) break;
                    str.push_back(static_cast<char16_t>(ch));
                }
                prop.value = std::move(str);
            } else {
                // Strip trailing null if present
                auto str_len = len;
                while (str_len > 0 && data[val_off + 4 + str_len - 1] == 0) --str_len;
                prop.value = std::string(reinterpret_cast<const char*>(data.data() + val_off + 4), str_len);
            }
        }
        break;

    case vt::lpwstr:
        if (val_off + 4 > data.size()) return std::unexpected(error::corrupt_file);
        {
            uint32_t char_count = read_u32(data.data() + val_off);
            uint32_t byte_count = char_count * 2;
            if (val_off + 4 + byte_count > data.size()) return std::unexpected(error::corrupt_file);
            std::u16string str;
            str.reserve(char_count);
            for (uint32_t i = 0; i < char_count; ++i) {
                auto ch = read_u16(data.data() + val_off + 4 + i * 2);
                if (ch == 0) break;
                str.push_back(static_cast<char16_t>(ch));
            }
            prop.value = std::move(str);
        }
        break;

    case vt::blob:
        if (val_off + 4 > data.size()) return std::unexpected(error::corrupt_file);
        {
            uint32_t len = read_u32(data.data() + val_off);
            if (val_off + 4 + len > data.size()) return std::unexpected(error::corrupt_file);
            prop.value = std::vector<uint8_t>(data.data() + val_off + 4,
                                               data.data() + val_off + 4 + len);
        }
        break;

    case vt::clsid:
        if (val_off + 16 > data.size()) return std::unexpected(error::corrupt_file);
        prop.value = guid_read_le(data.data() + val_off);
        break;

    default:
        // Unknown type — store as empty
        prop.value = std::monostate{};
        break;
    }

    return prop;
}

// ── Parse a property section ───────────────────────────────────────────

static auto parse_section(std::span<const uint8_t> data, uint32_t section_offset,
                           const guid& fmtid)
    -> std::expected<property_section, error> {
    if (section_offset + 8 > data.size()) return std::unexpected(error::corrupt_file);

    property_section sec;
    sec.fmtid = fmtid;

    uint32_t section_size = read_u32(data.data() + section_offset);
    uint32_t num_props = read_u32(data.data() + section_offset + 4);

    if (section_offset + 8 + num_props * 8 > data.size())
        return std::unexpected(error::corrupt_file);

    // First pass: find codepage so string properties are decoded correctly
    for (uint32_t i = 0; i < num_props; ++i) {
        uint32_t pid_val = read_u32(data.data() + section_offset + 8 + i * 8);
        if (pid_val == pid::codepage) {
            uint32_t prop_offset = read_u32(data.data() + section_offset + 8 + i * 8 + 4);
            uint32_t abs_offset = section_offset + prop_offset;
            if (abs_offset + 8 <= data.size()) {
                sec.codepage = static_cast<uint16_t>(read_u32(data.data() + abs_offset + 4));
            }
            break;
        }
    }

    // Second pass: parse all properties with the correct codepage
    for (uint32_t i = 0; i < num_props; ++i) {
        uint32_t pid_val = read_u32(data.data() + section_offset + 8 + i * 8);
        uint32_t prop_offset = read_u32(data.data() + section_offset + 8 + i * 8 + 4);
        uint32_t abs_offset = section_offset + prop_offset;

        auto result = parse_typed_value(data, abs_offset, sec.codepage);
        if (result) {
            result->id = pid_val;
            sec.properties[pid_val] = std::move(*result);
        }
    }

    return sec;
}

// ── parse_property_set ─────────────────────────────────────────────────

auto parse_property_set(std::span<const uint8_t> data)
    -> std::expected<property_set, error> {
    // Minimum: header (28 bytes) + at least 1 section entry (20 bytes)
    if (data.size() < 28) return std::unexpected(error::corrupt_file);

    property_set ps;
    ps.byte_order = read_u16(data.data());
    ps.format_version = read_u16(data.data() + 2);
    ps.os_version = read_u32(data.data() + 4);
    ps.clsid = guid_read_le(data.data() + 8);
    uint32_t num_sections = read_u32(data.data() + 24);

    if (num_sections == 0 || num_sections > 2)
        return std::unexpected(error::corrupt_file);

    if (28 + num_sections * 20 > data.size())
        return std::unexpected(error::corrupt_file);

    for (uint32_t i = 0; i < num_sections; ++i) {
        guid fmtid = guid_read_le(data.data() + 28 + i * 20);
        uint32_t offset = read_u32(data.data() + 28 + i * 20 + 16);

        auto sec = parse_section(data, offset, fmtid);
        if (!sec) return std::unexpected(sec.error());
        ps.sections.push_back(std::move(*sec));
    }

    return ps;
}

// ── Serialize a typed value ────────────────────────────────────────────

static auto serialize_typed_value(const property& prop, std::vector<uint8_t>& out) {
    auto start = out.size();
    // Write type (4 bytes: 2 type + 2 padding)
    out.resize(out.size() + 4);
    write_u16(out.data() + start, static_cast<uint16_t>(prop.type));
    write_u16(out.data() + start + 2, 0); // padding

    switch (prop.type) {
    case vt::empty:
    case vt::null_:
        break;

    case vt::i2:
    case vt::bool_: {
        out.resize(out.size() + 4); // 2 bytes + 2 padding
        auto val = std::get_if<int16_t>(&prop.value);
        write_u16(out.data() + start + 4, val ? static_cast<uint16_t>(*val) : 0);
        write_u16(out.data() + start + 6, 0);
        break;
    }

    case vt::i4: {
        out.resize(out.size() + 4);
        auto val = std::get_if<int32_t>(&prop.value);
        write_u32(out.data() + start + 4, val ? static_cast<uint32_t>(*val) : 0);
        break;
    }

    case vt::ui2: {
        out.resize(out.size() + 4);
        auto val = std::get_if<uint16_t>(&prop.value);
        write_u16(out.data() + start + 4, val ? *val : 0);
        write_u16(out.data() + start + 6, 0);
        break;
    }

    case vt::ui4: {
        out.resize(out.size() + 4);
        auto val = std::get_if<uint32_t>(&prop.value);
        write_u32(out.data() + start + 4, val ? *val : 0);
        break;
    }

    case vt::i8:
    case vt::cy: {
        out.resize(out.size() + 8);
        auto val = std::get_if<int64_t>(&prop.value);
        write_u64(out.data() + start + 4, val ? static_cast<uint64_t>(*val) : 0);
        break;
    }

    case vt::ui8:
    case vt::filetime: {
        out.resize(out.size() + 8);
        auto val = std::get_if<uint64_t>(&prop.value);
        write_u64(out.data() + start + 4, val ? *val : 0);
        break;
    }

    case vt::r4: {
        out.resize(out.size() + 4);
        auto val = std::get_if<float>(&prop.value);
        float f = val ? *val : 0.0f;
        std::memcpy(out.data() + start + 4, &f, 4);
        break;
    }

    case vt::r8:
    case vt::date: {
        out.resize(out.size() + 8);
        auto val = std::get_if<double>(&prop.value);
        double d = val ? *val : 0.0;
        std::memcpy(out.data() + start + 4, &d, 8);
        break;
    }

    case vt::lpstr:
    case vt::bstr: {
        auto val = std::get_if<std::string>(&prop.value);
        std::string s = val ? *val : "";
        uint32_t len = static_cast<uint32_t>(s.size() + 1); // include null
        uint32_t padded = pad4(len);
        out.resize(out.size() + 4 + padded, 0);
        write_u32(out.data() + start + 4, len);
        std::memcpy(out.data() + start + 8, s.data(), s.size());
        break;
    }

    case vt::lpwstr: {
        auto val = std::get_if<std::u16string>(&prop.value);
        std::u16string s = val ? *val : u"";
        uint32_t char_count = static_cast<uint32_t>(s.size() + 1); // include null
        uint32_t byte_count = char_count * 2;
        uint32_t padded = pad4(byte_count);
        out.resize(out.size() + 4 + padded, 0);
        write_u32(out.data() + start + 4, char_count);
        for (uint32_t i = 0; i < s.size(); ++i) {
            write_u16(out.data() + start + 8 + i * 2, static_cast<uint16_t>(s[i]));
        }
        break;
    }

    case vt::blob: {
        auto val = std::get_if<std::vector<uint8_t>>(&prop.value);
        uint32_t len = val ? static_cast<uint32_t>(val->size()) : 0;
        uint32_t padded = pad4(len);
        out.resize(out.size() + 4 + padded, 0);
        write_u32(out.data() + start + 4, len);
        if (val && !val->empty()) {
            std::memcpy(out.data() + start + 8, val->data(), val->size());
        }
        break;
    }

    case vt::clsid: {
        out.resize(out.size() + 16);
        auto val = std::get_if<guid>(&prop.value);
        guid g = val ? *val : guid{};
        guid_write_le(out.data() + start + 4, g);
        break;
    }

    default:
        break;
    }
}

// ── serialize_property_set ─────────────────────────────────────────────

auto serialize_property_set(const property_set& ps)
    -> std::expected<std::vector<uint8_t>, error> {
    std::vector<uint8_t> out;

    uint32_t num_sections = static_cast<uint32_t>(ps.sections.size());
    uint32_t header_size = 28 + num_sections * 20;

    // Reserve header space
    out.resize(header_size, 0);

    // Write header
    write_u16(out.data(), ps.byte_order);
    write_u16(out.data() + 2, ps.format_version);
    write_u32(out.data() + 4, ps.os_version);
    guid_write_le(out.data() + 8, ps.clsid);
    write_u32(out.data() + 24, num_sections);

    // Serialize each section
    for (uint32_t si = 0; si < num_sections; ++si) {
        auto& sec = ps.sections[si];

        // Write FMTID + offset in the header's section list
        guid_write_le(out.data() + 28 + si * 20, sec.fmtid);
        uint32_t section_start = static_cast<uint32_t>(out.size());
        write_u32(out.data() + 28 + si * 20 + 16, section_start);

        // Section header: size (placeholder) + property count
        uint32_t num_props = static_cast<uint32_t>(sec.properties.size());
        uint32_t sec_header_size = 8 + num_props * 8;
        out.resize(out.size() + sec_header_size, 0);
        // num_props
        write_u32(out.data() + section_start + 4, num_props);

        // Serialize each property value and record offsets
        uint32_t prop_idx = 0;
        for (auto& [pid, prop] : sec.properties) {
            uint32_t prop_offset = static_cast<uint32_t>(out.size()) - section_start;

            // Write PID + offset in the property ID/offset array
            write_u32(out.data() + section_start + 8 + prop_idx * 8, pid);
            write_u32(out.data() + section_start + 8 + prop_idx * 8 + 4, prop_offset);

            serialize_typed_value(prop, out);
            ++prop_idx;
        }

        // Write section size
        uint32_t section_size = static_cast<uint32_t>(out.size()) - section_start;
        write_u32(out.data() + section_start, section_size);
    }

    return out;
}

} // namespace stout::ole
