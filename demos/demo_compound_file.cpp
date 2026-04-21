#include "stout/compound_file.h"
#include "stout/ole/property_set_storage.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <print>
#include <vector>

namespace fs = std::filesystem;

static void print_separator(const char *title) {
    std::println("\n--- {} ---", title);
}

int main() {
    auto tmp = fs::temp_directory_path() / "stout_demo.cfb";

    // ── Create a new compound file ──────────────────────────────────────
    print_separator("Create compound file");
    auto cf = stout::compound_file::create(tmp, stout::cfb_version::v4);
    if (!cf) {
        std::println("  ERROR: {}", stout::error_message(cf.error()));
        return 1;
    }
    std::println("  Created: {}", tmp.string());
    std::println("  Version: v{}", cf->version() == stout::cfb_version::v4 ? 4 : 3);

    auto root = cf->root_storage();

    // ── Storage metadata: CLSID, state bits, timestamps ─────────────────
    print_separator("Storage metadata");
    stout::guid app_clsid{0xDEADBEEF, 0xCAFE, 0xBABE, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08}};
    root.set_clsid(app_clsid);
    root.set_state_bits(0x42);
    auto now = std::chrono::system_clock::now();
    root.set_modified_time(now);

    std::println("  Root CLSID:      {}", stout::guid_to_string(root.clsid()));
    std::println("  Root state bits: 0x{:08X}", root.state_bits());
    std::println("  Modified time set to now");

    // ── Create storages (folders) ───────────────────────────────────────
    print_separator("Create storages");
    auto docs = root.create_storage("Documents");
    auto imgs = root.create_storage("Images");
    if (!docs || !imgs) {
        std::println("  ERROR creating storages");
        return 1;
    }
    std::println("  Created: Documents, Images");

    // ── Create and write streams ────────────────────────────────────────
    print_separator("Write streams");

    // A regular-sized stream
    auto readme = docs->create_stream("readme.txt");
    if (!readme) {
        std::println("  ERROR: {}", stout::error_message(readme.error()));
        return 1;
    }
    std::string text = "Hello from Stout! This is a compound document stream.";
    auto wr = readme->write(0, std::span<const uint8_t>(reinterpret_cast<const uint8_t *>(text.data()), text.size()));
    std::println("  readme.txt: wrote {} bytes", wr.has_value() ? *wr : 0);

    // A mini stream (< 4096 bytes)
    auto icon = imgs->create_stream("icon.bin");
    if (!icon) {
        std::println("  ERROR: {}", stout::error_message(icon.error()));
        return 1;
    }
    std::vector<uint8_t> icon_data(256);
    for (size_t i = 0; i < icon_data.size(); ++i) {
        icon_data[i] = static_cast<uint8_t>(i);
    }
    icon->write(0, std::span<const uint8_t>(icon_data));
    std::println("  icon.bin:   wrote {} bytes (mini stream)", icon_data.size());

    // A large stream that starts mini and gets resized to regular
    auto log_stream = docs->create_stream("activity.log");
    if (!log_stream) {
        std::println("  ERROR: {}", stout::error_message(log_stream.error()));
        return 1;
    }
    std::vector<uint8_t> small_data(100, 0xAA);
    log_stream->write(0, std::span<const uint8_t>(small_data));
    std::println("  activity.log: wrote 100 bytes (mini)");

    // ── Resize: mini → regular migration ────────────────────────────────
    print_separator("Resize mini -> regular");
    auto rr = log_stream->resize(5000);
    std::println("  Resized activity.log to 5000 bytes: {}", rr.has_value() ? "OK" : "FAIL");

    // Verify original data survived migration
    std::vector<uint8_t> verify(100);
    log_stream->read(0, std::span<uint8_t>(verify));
    bool data_ok = (verify == small_data);
    std::println("  Data integrity after migration: {}", data_ok ? "PASS" : "FAIL");

    // ── Rename ──────────────────────────────────────────────────────────
    print_separator("Rename");
    auto rename_r = docs->rename("Files");
    std::println("  Renamed 'Documents' -> 'Files': {}", rename_r.has_value() ? "OK" : "FAIL");

    // ── Property sets ───────────────────────────────────────────────────
    print_separator("Property sets");
    auto ps = stout::ole::make_summary_info("Demo Document", "Stout Demo", "Stout Library", "stout-demo");
    auto ps_wr = stout::ole::write_summary_info(root, ps);
    std::println("  Wrote SummaryInformation: {}", ps_wr.has_value() ? "OK" : "FAIL");

    // ── Flush and close ────────────────────────────────────────────────
    print_separator("Flush and close");
    auto fl = cf->flush();
    std::println("  Flush: {}", fl.has_value() ? "OK" : "FAIL");
    { auto _ = std::move(cf); } // close the file so we can reopen it
    std::println("  Closed");

    // ── Reopen and read back ────────────────────────────────────────────
    print_separator("Reopen and read");
    auto cf2 = stout::compound_file::open(tmp, stout::open_mode::read);
    if (!cf2) {
        std::println("  ERROR: {}", stout::error_message(cf2.error()));
        return 1;
    }

    auto root2 = cf2->root_storage();
    std::println("  Root CLSID: {}", stout::guid_to_string(root2.clsid()));
    std::println("  Root state bits: 0x{:08X}", root2.state_bits());

    // Enumerate children
    auto children = root2.children();
    std::println("  Root children ({}):", children.size());
    for (auto &c : children) {
        const char *type_str = c.type == stout::entry_type::storage ? "storage" : "stream";
        std::println("    {} [{}] size={}", c.name, type_str, c.size);
    }

    // Read the renamed storage
    auto files = root2.open_storage("Files");
    if (files) {
        auto file_children = files->children();
        std::println("  Files/ children ({}):", file_children.size());
        for (auto &c : file_children) {
            std::println("    {} size={}", c.name, c.size);
        }

        // Read readme.txt back
        auto rd = files->open_stream("readme.txt");
        if (rd) {
            std::vector<uint8_t> buf(static_cast<size_t>(rd->size()));
            rd->read(0, std::span<uint8_t>(buf));
            std::string content(buf.begin(), buf.end());
            std::println("  readme.txt content: \"{}\"", content);
        }
    }

    // Read property set back
    auto ps_rd = stout::ole::read_summary_info(root2);
    if (ps_rd) {
        auto *sec = ps_rd->section(stout::ole::fmtid_summary_information());
        if (sec) {
            std::println("  SummaryInfo.Title:   \"{}\"", sec->get_string(stout::ole::pid::title));
            std::println("  SummaryInfo.Author:  \"{}\"", sec->get_string(stout::ole::pid::author));
            std::println("  SummaryInfo.AppName: \"{}\"", sec->get_string(stout::ole::pid::app_name));
        }
    }

    // ── In-memory compound file ─────────────────────────────────────────
    print_separator("In-memory compound file");
    auto mem_cf = stout::compound_file::create_in_memory(stout::cfb_version::v4);
    if (!mem_cf) {
        std::println("  ERROR");
        return 1;
    }
    auto mem_root = mem_cf->root_storage();
    auto ms = mem_root.create_stream("data");
    if (ms) {
        std::vector<uint8_t> payload{0xDE, 0xAD, 0xBE, 0xEF};
        ms->write(0, std::span<const uint8_t>(payload));
    }
    mem_cf->flush();
    auto *raw = mem_cf->data();
    std::println("  In-memory file size: {} bytes", raw ? raw->size() : 0);

    // ── Cleanup ─────────────────────────────────────────────────────────
    print_separator("Cleanup");
    std::error_code ec;
    fs::remove(tmp, ec);
    std::println("  Removed temp file: {}", !ec ? "OK" : ec.message());

    std::println("\n=== Done ===");
    return 0;
}
