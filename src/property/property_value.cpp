#include "stout/ole/property_set_storage.h"

#include <format>
#include <sstream>

namespace stout::ole {

auto vt_type_name(vt type) noexcept -> const char * {
    switch (type) {
    case vt::empty:
        return "VT_EMPTY";
    case vt::null_:
        return "VT_NULL";
    case vt::i2:
        return "VT_I2";
    case vt::i4:
        return "VT_I4";
    case vt::r4:
        return "VT_R4";
    case vt::r8:
        return "VT_R8";
    case vt::cy:
        return "VT_CY";
    case vt::date:
        return "VT_DATE";
    case vt::bstr:
        return "VT_BSTR";
    case vt::bool_:
        return "VT_BOOL";
    case vt::ui2:
        return "VT_UI2";
    case vt::ui4:
        return "VT_UI4";
    case vt::i8:
        return "VT_I8";
    case vt::ui8:
        return "VT_UI8";
    case vt::lpstr:
        return "VT_LPSTR";
    case vt::lpwstr:
        return "VT_LPWSTR";
    case vt::filetime:
        return "VT_FILETIME";
    case vt::blob:
        return "VT_BLOB";
    case vt::clsid:
        return "VT_CLSID";
    default:
        return "VT_UNKNOWN";
    }
}

auto property_value_to_string(const property &prop) -> std::string {
    return std::visit(
        [&](auto &&val) -> std::string {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
                return "(empty)";
            } else if constexpr (std::is_same_v<T, int16_t>) {
                if (prop.type == vt::bool_) {
                    return val != 0 ? "true" : "false";
                }
                return std::to_string(val);
            } else if constexpr (std::is_same_v<T, int32_t>) {
                return std::to_string(val);
            } else if constexpr (std::is_same_v<T, uint16_t>) {
                return std::to_string(val);
            } else if constexpr (std::is_same_v<T, uint32_t>) {
                return std::to_string(val);
            } else if constexpr (std::is_same_v<T, int64_t>) {
                return std::to_string(val);
            } else if constexpr (std::is_same_v<T, uint64_t>) {
                if (prop.type == vt::filetime) {
                    return std::format("0x{:016X}", val);
                }
                return std::to_string(val);
            } else if constexpr (std::is_same_v<T, float>) {
                return std::to_string(val);
            } else if constexpr (std::is_same_v<T, double>) {
                return std::to_string(val);
            } else if constexpr (std::is_same_v<T, std::string>) {
                return "\"" + val + "\"";
            } else if constexpr (std::is_same_v<T, std::u16string>) {
                // Simple ASCII approximation for debug
                std::string result = "u\"";
                for (auto ch : val) {
                    if (ch < 128) {
                        result += static_cast<char>(ch);
                    } else {
                        result += '?';
                    }
                }
                result += "\"";
                return result;
            } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
                return std::format("blob[{} bytes]", val.size());
            } else if constexpr (std::is_same_v<T, guid>) {
                return guid_to_string(val);
            } else {
                return "(unknown)";
            }
        },
        prop.value);
}

} // namespace stout::ole
