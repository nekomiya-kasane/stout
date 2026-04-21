#include "stout/ole/property_set_storage.h"

namespace stout::ole {

auto make_summary_info(std::string_view title, std::string_view subject, std::string_view author,
                       std::string_view app_name) -> property_set {
    property_set ps;
    ps.byte_order = 0xFFFE;
    ps.format_version = 0;
    ps.os_version = 0x00060002; // Windows 6.2
    ps.clsid = {};

    auto &sec = ps.add_section(fmtid_summary_information());
    sec.codepage = 1252;
    // Always write codepage property
    sec.set(pid::codepage, vt::i2, static_cast<int16_t>(1252));

    if (!title.empty()) {
        sec.set_string(pid::title, std::string(title));
    }
    if (!subject.empty()) {
        sec.set_string(pid::subject, std::string(subject));
    }
    if (!author.empty()) {
        sec.set_string(pid::author, std::string(author));
    }
    if (!app_name.empty()) {
        sec.set_string(pid::app_name, std::string(app_name));
    }

    return ps;
}

} // namespace stout::ole
