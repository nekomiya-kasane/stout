/**
 * @file main.cpp
 * @brief ss_viewer entry point — CLI parsing, file open, mode dispatch.
 *
 * Usage:
 *   ss_viewer.exe <file.cfb>              — open with stout backend
 *   ss_viewer.exe --win32 <file.cfb>      — open with Win32 IStorage backend
 *   ss_viewer.exe --dump <file.cfb>       — render one frame to stdout
 *   ss_viewer.exe --dump-canvas <file.cfb> — render canvas cells to stdout
 *
 * Controls:
 *   Up/Down        — navigate tree
 *   Enter/Right    — expand / select
 *   Left/Backspace — collapse / go to parent
 *   Tab/Shift+Tab  — switch detail tabs (Info / Hex / Properties)
 *   Page Up/Down   — scroll hex dump
 *   q / Escape     — quit
 */

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "ss_viewer/actions/key_handler.h"
#include "ss_viewer/actions/menu_handler.h"
#include "ss_viewer/model/entry_info.h"
#include "ss_viewer/model/stout_backend.h"
#include "ss_viewer/model/viewer_state.h"
#include "ss_viewer/model/win32_backend.h"
#include "ss_viewer/ui/frame_builder.h"
#include "ss_viewer/ui/menus.h"
#include "ss_viewer/util/format.h"
#include "stout/compound_file.h"
#include "tapiru/core/console.h"
#include "tapiru/widgets/classic_app.h"

#include <Windows.h>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <iostream>
#include <objbase.h>
#include <string>

namespace fs = std::filesystem;
using namespace tapiru;

int main(int argc, char *argv[]) {
    // ── Parse command line ──
    if (argc < 2) {
        std::fprintf(stderr, "Usage: ss_viewer [--win32] [--dump] [--dump-canvas] <file.cfb>\n");
        return 1;
    }

    ssv::viewer_state st;
    bool dump_mode = false;
    bool dump_canvas_mode = false;
    uint32_t dump_width = 120;
    uint32_t dump_height = 30;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--win32") {
            st.use_win32 = true;
        } else if (arg == "--dump") {
            dump_mode = true;
        } else if (arg == "--dump-canvas") {
            dump_canvas_mode = true;
        } else if (arg == "--width" && i + 1 < argc) {
            dump_width = static_cast<uint32_t>(std::atoi(argv[++i]));
        } else if (arg == "--height" && i + 1 < argc) {
            dump_height = static_cast<uint32_t>(std::atoi(argv[++i]));
        } else {
            st.file_path = arg;
        }
    }

    if (st.file_path.empty()) {
        std::fprintf(stderr, "Error: no file specified\n");
        return 1;
    }
    if (!fs::exists(st.file_path)) {
        std::fprintf(stderr, "Error: file not found: %s\n", st.file_path.string().c_str());
        return 1;
    }
    st.file_size = fs::file_size(st.file_path);

    // ── Open file ──
    if (st.use_win32) {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        auto wpath = st.file_path.wstring();
        HRESULT hr =
            StgOpenStorage(wpath.c_str(), nullptr, STGM_READ | STGM_SHARE_DENY_WRITE, nullptr, 0, &st.root_stg);
        if (FAILED(hr)) {
            std::fprintf(stderr, "Error: StgOpenStorage failed (0x%08lX)\n", hr);
            return 1;
        }
        st.root_entry = ssv::build_win32_tree(st.root_stg, "", "Root Entry");
        st.version_str = "Win32";
        st.sector_str = "N/A";
    } else {
        auto result = stout::compound_file::open(st.file_path, stout::open_mode::read);
        if (!result) {
            std::fprintf(stderr, "Error: failed to open: %s\n", stout::error_message(result.error()));
            return 1;
        }
        st.cf.emplace(std::move(*result));
        auto root = st.cf->root_storage();
        st.root_entry = ssv::build_stout_tree(root, "");
        st.version_str = st.cf->version() == stout::cfb_version::v4 ? "v4" : "v3";
        uint32_t sec_sz = st.cf->version() == stout::cfb_version::v4 ? 4096 : 512;
        st.sector_str = std::to_string(sec_sz) + " B";
    }

    // Expand root by default
    st.expanded.insert(ssv::tree_label(st.root_entry));
    st.rebuild_flat_paths();
    st.selected = &st.root_entry;

    // ── Dump mode ──
    if (dump_mode) {
        int viewport_h = static_cast<int>(dump_height) - 3;
        auto theme = classic_app_theme::dark();
        auto frame = ssv::build_frame(st, viewport_h, theme);

        console_config ccfg;
        ccfg.sink = [](std::string_view data) { std::fwrite(data.data(), 1, data.size(), stdout); };
        ccfg.depth = color_depth::true_color;
        ccfg.force_color = true;
        console con(ccfg);
        std::fprintf(stderr, "=== DUMP: %ux%u (viewport_h=%d) ===\n", dump_width, dump_height, viewport_h);
        std::fprintf(stderr, "expanded set (%zu entries):\n", st.expanded.size());
        for (auto &e : st.expanded) {
            std::fprintf(stderr, "  [%s]\n", e.c_str());
        }
        std::fprintf(stderr, "flat_paths (%zu entries):\n", st.flat_paths.size());
        for (size_t i = 0; i < st.flat_paths.size(); ++i) {
            std::fprintf(stderr, "  [%zu] %s%s\n", i, st.flat_paths[i].c_str(),
                         (static_cast<int>(i) == st.tree_cursor) ? " <-- cursor" : "");
        }
        std::fprintf(stderr, "tree_cursor=%d selected=%s\n", st.tree_cursor,
                     st.selected ? st.selected->full_path.c_str() : "(null)");
        std::fprintf(stderr, "=== RENDERED FRAME ===\n");
        con.print_widget(frame, dump_width);

        if (st.use_win32) {
            CoUninitialize();
        }
        return 0;
    }

    // ── Dump-canvas mode ──
    if (dump_canvas_mode) {
        int viewport_h = static_cast<int>(dump_height) - 3;
        auto theme = classic_app_theme::dark();
        auto frame = ssv::build_frame(st, viewport_h, theme);

        auto wc = render_to_canvas(frame, dump_width);

        std::fprintf(stderr, "=== DUMP-CANVAS: %ux%u (viewport_h=%d) canvas=%ux%u ===\n", dump_width, dump_height,
                     viewport_h, wc.width, wc.height);
        wc.dump(std::cout);

        // Styled info for key rows
        std::fprintf(stderr, "--- styled info for key rows ---\n");
        auto dump_row_style = [&](uint32_t y) {
            if (y >= wc.height) {
                return;
            }
            std::fprintf(stderr, "row %u: ", y);
            for (uint32_t x = 0; x < std::min(wc.width, 40u); ++x) {
                auto &c = wc.cell_at(x, y);
                if (c.width == 0) {
                    continue;
                }
                char32_t cp = c.codepoint;
                if (cp == 0 || cp == U' ') {
                    std::fprintf(stderr, ".");
                } else if (cp < 128) {
                    std::fprintf(stderr, "%c", static_cast<char>(cp));
                } else {
                    std::fprintf(stderr, "U+%04X", static_cast<unsigned>(cp));
                }
            }
            std::fprintf(stderr, "\n");
            for (uint32_t x = 0; x < std::min(wc.width, 40u); ++x) {
                auto &c = wc.cell_at(x, y);
                if (c.sid != default_style_id && c.width > 0) {
                    auto sty = wc.style_at(x, y);
                    std::fprintf(stderr, "  [%u,%u] fg=(%u,%u,%u) bg=(%u,%u,%u)\n", x, y, sty.fg.r, sty.fg.g, sty.fg.b,
                                 sty.bg.r, sty.bg.g, sty.bg.b);
                }
            }
        };
        dump_row_style(0);
        dump_row_style(1);
        dump_row_style(2);
        if (wc.height > 0) {
            dump_row_style(wc.height - 1);
        }

        if (st.use_win32) {
            CoUninitialize();
        }
        return 0;
    }

    // ── Interactive mode ──
    classic_app app(classic_app::config{
        .menus = ssv::build_menus(),
        .theme = classic_app_theme::dark(),
        .poll_interval_ms = 50,
    });

    app.set_content([&](rows_builder &content, int /*scroll_y*/, int viewport_h) {
        app.set_content_lines(viewport_h);
        content.add(ssv::build_content(st, viewport_h));
    });

    app.set_status([&](status_bar_builder &sb) { ssv::build_status(st, sb); });

    app.on_menu_action(
        [&](int /*menu_idx*/, int /*item_idx*/, const std::string &label) { ssv::handle_menu(label, st, app); });

    app.on_key([&](const key_event &ke) -> bool { return ssv::handle_key(ke, st, app); });

    app.run();

    if (st.use_win32) {
        CoUninitialize();
    }
    return 0;
}
