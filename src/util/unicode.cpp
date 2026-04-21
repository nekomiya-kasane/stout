#include "stout/util/unicode.h"

#include <algorithm>
#include <cstring>
#include <optional>

namespace stout::util {

auto utf8_to_utf16le(std::string_view utf8) -> std::u16string {
    std::u16string result;
    result.reserve(utf8.size());

    size_t i = 0;
    while (i < utf8.size()) {
        uint32_t cp = 0;
        auto ch = static_cast<uint8_t>(utf8[i]);

        if (ch < 0x80) {
            cp = ch;
            i += 1;
        } else if ((ch & 0xE0) == 0xC0) {
            if (i + 1 >= utf8.size()) {
                break;
            }
            cp = (ch & 0x1F) << 6;
            cp |= (static_cast<uint8_t>(utf8[i + 1]) & 0x3F);
            i += 2;
        } else if ((ch & 0xF0) == 0xE0) {
            if (i + 2 >= utf8.size()) {
                break;
            }
            cp = (ch & 0x0F) << 12;
            cp |= (static_cast<uint8_t>(utf8[i + 1]) & 0x3F) << 6;
            cp |= (static_cast<uint8_t>(utf8[i + 2]) & 0x3F);
            i += 3;
        } else if ((ch & 0xF8) == 0xF0) {
            if (i + 3 >= utf8.size()) {
                break;
            }
            cp = (ch & 0x07) << 18;
            cp |= (static_cast<uint8_t>(utf8[i + 1]) & 0x3F) << 12;
            cp |= (static_cast<uint8_t>(utf8[i + 2]) & 0x3F) << 6;
            cp |= (static_cast<uint8_t>(utf8[i + 3]) & 0x3F);
            i += 4;
        } else {
            ++i;
            continue;
        }

        if (cp <= 0xFFFF) {
            result.push_back(static_cast<char16_t>(cp));
        } else if (cp <= 0x10FFFF) {
            cp -= 0x10000;
            result.push_back(static_cast<char16_t>(0xD800 | (cp >> 10)));
            result.push_back(static_cast<char16_t>(0xDC00 | (cp & 0x3FF)));
        }
    }

    return result;
}

auto utf16le_to_utf8(std::u16string_view utf16) -> std::string {
    std::string result;
    result.reserve(utf16.size() * 3);

    size_t i = 0;
    while (i < utf16.size()) {
        uint32_t cp = utf16[i];

        if (cp >= 0xD800 && cp <= 0xDBFF) {
            if (i + 1 < utf16.size()) {
                uint32_t lo = utf16[i + 1];
                if (lo >= 0xDC00 && lo <= 0xDFFF) {
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    ++i;
                }
            }
        }
        ++i;

        if (cp < 0x80) {
            result.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            result.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            result.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            result.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            result.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            result.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    return result;
}

auto dir_name_to_utf8(const uint8_t *name_bytes, uint16_t byte_count) -> std::string {
    if (byte_count < 2) {
        return {};
    }

    // byte_count includes the null terminator (2 bytes for UTF-16)
    uint16_t char_count = (byte_count / 2) - 1;
    std::u16string u16;
    u16.reserve(char_count);

    for (uint16_t i = 0; i < char_count; ++i) {
        auto lo = name_bytes[i * 2];
        auto hi = name_bytes[i * 2 + 1];
        u16.push_back(static_cast<char16_t>(lo | (hi << 8)));
    }

    return utf16le_to_utf8(u16);
}

auto utf8_to_dir_name(std::string_view name) -> std::optional<dir_name_result> {
    if (!cfb_name_is_valid(name)) {
        return std::nullopt;
    }

    auto u16 = utf8_to_utf16le(name);
    if (u16.size() >= 32) {
        return std::nullopt; // max 31 chars + null
    }

    dir_name_result result;
    for (size_t i = 0; i < u16.size(); ++i) {
        result.bytes[i * 2] = static_cast<uint8_t>(u16[i] & 0xFF);
        result.bytes[i * 2 + 1] = static_cast<uint8_t>(u16[i] >> 8);
    }
    // Null terminator
    result.bytes[u16.size() * 2] = 0;
    result.bytes[u16.size() * 2 + 1] = 0;
    result.byte_count = static_cast<uint16_t>((u16.size() + 1) * 2);

    return result;
}

auto cfb_toupper(char16_t ch) noexcept -> char16_t {
    // Simple ASCII uppercase + basic Latin supplement
    if (ch >= u'a' && ch <= u'z') {
        return static_cast<char16_t>(ch - u'a' + u'A');
    }
    // Latin-1 supplement: à-ö (0xE0-0xF6) -> À-Ö (0xC0-0xD6)
    if (ch >= 0x00E0 && ch <= 0x00F6) {
        return static_cast<char16_t>(ch - 0x20);
    }
    // Latin-1 supplement: ø-þ (0xF8-0xFE) -> Ø-Þ (0xD8-0xDE)
    if (ch >= 0x00F8 && ch <= 0x00FE) {
        return static_cast<char16_t>(ch - 0x20);
    }
    return ch;
}

auto cfb_name_compare(std::u16string_view a, std::u16string_view b) noexcept -> int {
    // CFB rule: shorter name < longer name
    if (a.size() != b.size()) {
        return (a.size() < b.size()) ? -1 : 1;
    }

    // Same length: compare uppercase codepoints
    for (size_t i = 0; i < a.size(); ++i) {
        auto ua = cfb_toupper(a[i]);
        auto ub = cfb_toupper(b[i]);
        if (ua != ub) {
            return (ua < ub) ? -1 : 1;
        }
    }

    return 0;
}

auto cfb_name_is_valid(std::string_view name) noexcept -> bool {
    if (name.empty()) {
        return false;
    }

    for (char ch : name) {
        if (ch == '/' || ch == '\\' || ch == ':' || ch == '!') {
            return false;
        }
    }

    // Check length after conversion
    // We can't do full UTF-16 conversion here without allocation,
    // but a quick upper bound: UTF-8 bytes <= 4 * UTF-16 chars
    // The actual check is done in utf8_to_dir_name
    return true;
}

} // namespace stout::util
