#include "stout/stout.h"
#include <print>

int main() {
    auto v = stout::library_version();
    std::println("Stout v{}.{}.{}", v.major, v.minor, v.patch);
    std::println("  Version string: {}", stout::library_version_string());

    // GUID demo
    auto g = stout::guid_generate();
    std::println("  Generated GUID: {}", g);

    auto parsed = stout::guid_parse("{D5CDD502-2E9C-101B-9397-08002B2CF9AE}");
    if (parsed) {
        std::println("  Parsed GUID:    {}", *parsed);
    }

    // Unicode demo
    auto u16 = stout::util::utf8_to_utf16le("Hello World");
    auto back = stout::util::utf16le_to_utf8(u16);
    std::println("  UTF-8 -> UTF-16 -> UTF-8: \"{}\"", back);

    // CFB constants
    std::println("  CFB signature: {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X}",
        stout::cfb::signature[0], stout::cfb::signature[1],
        stout::cfb::signature[2], stout::cfb::signature[3],
        stout::cfb::signature[4], stout::cfb::signature[5],
        stout::cfb::signature[6], stout::cfb::signature[7]);
    std::println("  V3 sector size: {} bytes", stout::cfb::sector_size_v3);
    std::println("  V4 sector size: {} bytes", stout::cfb::sector_size_v4);

    return 0;
}
