/**
 * @file stout_dashboard.cpp
 * @brief Dashboard comparing stout vs Microsoft Win32 Structured Storage.
 *
 * Real benchmarks: creates compound files via both stout and Win32 COM APIs,
 * measures latency, and displays results using tapiru's TUI widgets.
 *
 * Build (from stout/):
 *   cmake --build build --target stout_dashboard --config Release
 * Run:
 *   .\build\bin\Release\stout_dashboard.exe
 */

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

// ── stout ────────────────────────────────────────────────────────────────
#include "stout/compound_file.h"
#include "stout/ole/property_set_storage.h"

// ── Win32 COM Structured Storage ─────────────────────────────────────────
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <objbase.h>
#include <propidl.h>
#include <windows.h>

// ── tapiru TUI ───────────────────────────────────────────────────────────
#include "tapiru/core/console.h"
#include "tapiru/core/decorator.h"
#include "tapiru/core/element.h"
#include "tapiru/core/style.h"
#include "tapiru/text/emoji.h"
#include "tapiru/widgets/builders.h"
#include "tapiru/widgets/canvas_widget.h"
#include "tapiru/widgets/chart.h"
#include "tapiru/widgets/gauge.h"
#include "tapiru/widgets/progress.h"
#include "tapiru/widgets/status_bar.h"

using namespace tapiru;
namespace fs = std::filesystem;

static constexpr int W = 80;

// ── RAII COM initializer ─────────────────────────────────────────────────
struct com_guard {
    com_guard() { CoInitializeEx(nullptr, COINIT_MULTITHREADED); }
    ~com_guard() { CoUninitialize(); }
};

// ── Benchmark helper ─────────────────────────────────────────────────────
struct bench_result {
    const char *name;
    double stout_us;
    double win32_us;
};

static void section(console &con, const char *title) {
    con.newline();
    con.print_widget(rule_builder(title).rule_style(style{colors::bright_cyan, {}, attr::bold}), W);
    con.newline();
}

// ═══════════════════════════════════════════════════════════════════════
//  Header
// ═══════════════════════════════════════════════════════════════════════

static void show_header(console &con) {
    con.newline();
    con.print_widget(rule_builder(" stout vs Win32 — Feature & Performance Dashboard ")
                         .rule_style(style{colors::bright_yellow, {}, attr::bold})
                         .character(U'\x2550'),
                     W);
    con.newline();

    rows_builder info;
    info.add(text_builder("[bold bright_white]stout[/] [dim]v1.0[/]  —  C++23 Compound File Binary (CFB) Library"));
    info.add(text_builder("[dim]vs[/]  [bold bright_white]Win32 Structured Storage[/] [dim](ole32.dll COM API)[/]"));
    info.add(text_builder("[dim]Side-by-side feature comparison and real benchmark results[/]"));
    info.gap(0);

    panel_builder pb(std::move(info));
    pb.title("About");
    pb.border(border_style::rounded);
    pb.border_style_override(style{colors::bright_cyan});
    con.print_widget(std::move(pb), W);
}

// ═══════════════════════════════════════════════════════════════════════
//  API Ergonomics Comparison
// ═══════════════════════════════════════════════════════════════════════

static void show_api_comparison(console &con) {
    section(con, " API Ergonomics Comparison ");

    table_builder tb;
    tb.add_column("Aspect", {justify::left, 20, 26});
    tb.add_column("stout (C++23)", {justify::left, 24, 30});
    tb.add_column("Win32 COM", {justify::left, 24, 30});
    tb.border(border_style::rounded);
    tb.header_style(style{colors::bright_yellow, {}, attr::bold});
    tb.shadow();

    tb.add_row({"Error handling", "std::expected<T,E>", "HRESULT + out-params"});
    tb.add_row({"Memory mgmt", "RAII / value types", "COM ref-counting"});
    tb.add_row({"String type", "std::string_view", "LPOLESTR (wchar_t*)"});
    tb.add_row({"Init boilerplate", "None", "CoInitialize + CoUninit"});
    tb.add_row({"Create file", "1 line", "StgCreateDocfile (5 args)"});
    tb.add_row({"Open file", "1 line", "StgOpenStorage (6 args)"});
    tb.add_row({"Create stream", "1 line", "CreateStream (5 args)"});
    tb.add_row({"Write data", "stream.write(off,buf)", "IStream::Write(3 args)"});
    tb.add_row({"Read data", "stream.read(off,buf)", "IStream::Read(3 args)"});
    tb.add_row({"Enumerate", "storage.children()", "EnumElements loop"});
    tb.add_row({"Stat query", "entry.stat()", "IStorage::Stat(2 args)"});
    tb.add_row({"Rename", "entry.rename(name)", "RenameElement(2 args)"});
    tb.add_row({"Delete", "storage.remove(name)", "DestroyElement(1 arg)"});
    tb.add_row({"Timestamps", "set_modified_time(t)", "SetElementTimes(4 args)"});
    tb.add_row({"Property sets", "read/write helpers", "IPropertySetStorage"});
    tb.add_row({"Transaction", "begin/commit/revert", "STGM_TRANSACTED flag"});
    tb.add_row({"In-memory", "create_in_memory()", "ILockBytes + custom"});
    tb.add_row({"Cross-platform", "[green]Yes[/]", "[red]Windows only[/]"});
    tb.add_row({"Header-only deps", "[green]None[/]", "windows.h + objbase.h"});
    tb.add_row({"C++ standard", "C++23", "C / COM"});

    con.print_widget(tb, W);
}

// ═══════════════════════════════════════════════════════════════════════
//  Feature Matrix Comparison
// ═══════════════════════════════════════════════════════════════════════

static void show_feature_matrix(console &con) {
    section(con, " Feature Matrix Comparison ");

    table_builder tb;
    tb.add_column("Feature", {justify::left, 28, 34});
    tb.add_column("stout", {justify::center, 10, 14});
    tb.add_column("Win32", {justify::center, 10, 14});
    tb.add_column("Notes", {justify::left, 16, 22});
    tb.border(border_style::rounded);
    tb.header_style(style{colors::bright_cyan, {}, attr::bold});

    auto y = "[green]Yes[/]";
    auto n = "[red]No[/]";

    tb.add_row({"Create CFB v3", y, y, ""});
    tb.add_row({"Create CFB v4", y, y, ""});
    tb.add_row({"Open existing CFB", y, y, ""});
    tb.add_row({"Nested storages", y, y, ""});
    tb.add_row({"Create/open streams", y, y, ""});
    tb.add_row({"Read/write at offset", y, y, ""});
    tb.add_row({"Mini stream (<4096)", y, y, "Auto-managed"});
    tb.add_row({"Regular stream (>=4096)", y, y, "Auto-managed"});
    tb.add_row({"Stream resize", y, y, ""});
    tb.add_row({"Mini<->regular migration", y, y, "On resize"});
    tb.add_row({"Enumerate children", y, y, ""});
    tb.add_row({"Remove entries", y, y, ""});
    tb.add_row({"Rename entries", y, y, ""});
    tb.add_row({"Exists check", y, y, ""});
    tb.add_row({"CLSID get/set", y, y, ""});
    tb.add_row({"State bits get/set", y, y, ""});
    tb.add_row({"Timestamps get/set", y, y, ""});
    tb.add_row({"SetElementTimes", y, y, ""});
    tb.add_row({"Copy entries", y, y, "Recursive"});
    tb.add_row({"OLE property sets", y, y, ""});
    tb.add_row({"SummaryInformation", y, y, ""});
    tb.add_row({"DocSummaryInformation", y, y, ""});
    tb.add_row({"Transaction support", y, y, ""});
    tb.add_row({"Flush to disk", y, y, ""});
    tb.add_row({"In-memory files", y, n, "[green]stout only[/]"});
    tb.add_row({"open_from_memory()", y, n, "[green]stout only[/]"});
    tb.add_row({"Cross-platform", y, n, "[green]stout only[/]"});
    tb.add_row({"No COM dependency", y, n, "[green]stout only[/]"});
    tb.add_row({"std::expected errors", y, n, "[green]stout only[/]"});
    tb.add_row({"RAII lifetime", y, n, "[green]stout only[/]"});
    tb.add_row({"IStorage COM interface", n, y, "[cyan]Win32 only[/]"});
    tb.add_row({"IStream COM interface", n, y, "[cyan]Win32 only[/]"});
    tb.add_row({"Structured Storage Viewer", n, y, "[cyan]Win32 only[/]"});

    con.print_widget(tb, W);
}

// ═══════════════════════════════════════════════════════════════════════
//  Real Performance Benchmarks
// ═══════════════════════════════════════════════════════════════════════

// ── stout benchmarks ─────────────────────────────────────────────────

static double bench_stout_create(const fs::path &path, int iters) {
    // Warmup
    for (int i = 0; i < 5; ++i) {
        auto cf = stout::compound_file::create(path, stout::cfb_version::v4);
        cf->flush();
    }
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iters; ++i) {
        auto cf = stout::compound_file::create(path, stout::cfb_version::v4);
        cf->flush();
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::micro>(t1 - t0).count() / iters;
}

static double bench_stout_create_stream(const fs::path &path, int iters) {
    auto cf = stout::compound_file::create(path, stout::cfb_version::v4);
    auto root = cf->root_storage();
    // Warmup
    for (int i = 0; i < 5; ++i) {
        char name[32];
        std::snprintf(name, sizeof(name), "warmup_%d", i);
        root.create_stream(name);
    }
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iters; ++i) {
        char name[32];
        std::snprintf(name, sizeof(name), "s%d", i);
        root.create_stream(name);
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::micro>(t1 - t0).count() / iters;
}

static double bench_stout_write_1k(const fs::path &path, int iters) {
    auto cf = stout::compound_file::create(path, stout::cfb_version::v4);
    auto root = cf->root_storage();
    auto strm = root.create_stream("data");
    std::vector<uint8_t> buf(1024, 0xAB);
    // Warmup
    for (int i = 0; i < 10; ++i) strm->write(0, std::span<const uint8_t>(buf));
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iters; ++i) {
        strm->write(0, std::span<const uint8_t>(buf));
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::micro>(t1 - t0).count() / iters;
}

static double bench_stout_write_8k(const fs::path &path, int iters) {
    auto cf = stout::compound_file::create(path, stout::cfb_version::v4);
    auto root = cf->root_storage();
    auto strm = root.create_stream("data");
    std::vector<uint8_t> buf(8192, 0xCD);
    // Warmup
    for (int i = 0; i < 10; ++i) strm->write(0, std::span<const uint8_t>(buf));
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iters; ++i) {
        strm->write(0, std::span<const uint8_t>(buf));
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::micro>(t1 - t0).count() / iters;
}

static double bench_stout_read_1k(const fs::path &path, int iters) {
    auto cf = stout::compound_file::create(path, stout::cfb_version::v4);
    auto root = cf->root_storage();
    auto strm = root.create_stream("data");
    std::vector<uint8_t> wbuf(1024, 0xAB);
    strm->write(0, std::span<const uint8_t>(wbuf));
    std::vector<uint8_t> rbuf(1024);
    // Warmup
    for (int i = 0; i < 10; ++i) strm->read(0, std::span<uint8_t>(rbuf));
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iters; ++i) {
        strm->read(0, std::span<uint8_t>(rbuf));
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::micro>(t1 - t0).count() / iters;
}

static double bench_stout_read_8k(const fs::path &path, int iters) {
    auto cf = stout::compound_file::create(path, stout::cfb_version::v4);
    auto root = cf->root_storage();
    auto strm = root.create_stream("data");
    std::vector<uint8_t> wbuf(8192, 0xCD);
    strm->write(0, std::span<const uint8_t>(wbuf));
    std::vector<uint8_t> rbuf(8192);
    // Warmup
    for (int i = 0; i < 10; ++i) strm->read(0, std::span<uint8_t>(rbuf));
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iters; ++i) {
        strm->read(0, std::span<uint8_t>(rbuf));
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::micro>(t1 - t0).count() / iters;
}

static double bench_stout_enumerate(const fs::path &path, int iters) {
    auto cf = stout::compound_file::create(path, stout::cfb_version::v4);
    auto root = cf->root_storage();
    for (int i = 0; i < 20; ++i) {
        char name[32];
        std::snprintf(name, sizeof(name), "child_%d", i);
        root.create_stream(name);
    }
    // Warmup
    for (int i = 0; i < 5; ++i) {
        volatile auto kids = root.children();
        (void)kids;
    }
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iters; ++i) {
        volatile auto kids = root.children();
        (void)kids;
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::micro>(t1 - t0).count() / iters;
}

static double bench_stout_property_set(const fs::path &path, int iters) {
    auto cf = stout::compound_file::create(path, stout::cfb_version::v4);
    auto root = cf->root_storage();
    auto ps = stout::ole::make_summary_info("Title", "Subject", "Author", "App");
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iters; ++i) {
        stout::ole::write_summary_info(root, ps);
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::micro>(t1 - t0).count() / iters;
}

// ── Win32 benchmarks ──────────────────────────────────────────────────────

static double bench_win32_create(const fs::path &path, int iters) {
    auto wpath = path.wstring();
    // Warmup
    for (int i = 0; i < 5; ++i) {
        IStorage *pStg = nullptr;
        StgCreateDocfile(wpath.c_str(), STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, &pStg);
        if (pStg) {
            pStg->Commit(STGC_DEFAULT);
            pStg->Release();
        }
    }
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iters; ++i) {
        IStorage *pStg = nullptr;
        StgCreateDocfile(wpath.c_str(), STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, &pStg);
        if (pStg) {
            pStg->Commit(STGC_DEFAULT);
            pStg->Release();
        }
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::micro>(t1 - t0).count() / iters;
}

static double bench_win32_create_stream(const fs::path &path, int iters) {
    auto wpath = path.wstring();
    IStorage *pStg = nullptr;
    StgCreateDocfile(wpath.c_str(), STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, &pStg);
    // Warmup
    for (int i = 0; i < 5; ++i) {
        wchar_t name[32];
        swprintf_s(name, L"warmup_%d", i);
        IStream *pStrm = nullptr;
        pStg->CreateStream(name, STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &pStrm);
        if (pStrm) pStrm->Release();
    }
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iters; ++i) {
        wchar_t name[32];
        swprintf_s(name, L"s%d", i);
        IStream *pStrm = nullptr;
        pStg->CreateStream(name, STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &pStrm);
        if (pStrm) pStrm->Release();
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    if (pStg) pStg->Release();
    return std::chrono::duration<double, std::micro>(t1 - t0).count() / iters;
}

static double bench_win32_write_1k(const fs::path &path, int iters) {
    auto wpath = path.wstring();
    IStorage *pStg = nullptr;
    StgCreateDocfile(wpath.c_str(), STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, &pStg);
    IStream *pStrm = nullptr;
    pStg->CreateStream(L"data", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &pStrm);
    std::vector<uint8_t> buf(1024, 0xAB);
    LARGE_INTEGER zero{};
    zero.QuadPart = 0;
    // Warmup
    for (int i = 0; i < 10; ++i) {
        pStrm->Seek(zero, STREAM_SEEK_SET, nullptr);
        ULONG w = 0;
        pStrm->Write(buf.data(), static_cast<ULONG>(buf.size()), &w);
    }
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iters; ++i) {
        pStrm->Seek(zero, STREAM_SEEK_SET, nullptr);
        ULONG written = 0;
        pStrm->Write(buf.data(), static_cast<ULONG>(buf.size()), &written);
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    if (pStrm) pStrm->Release();
    if (pStg) pStg->Release();
    return std::chrono::duration<double, std::micro>(t1 - t0).count() / iters;
}

static double bench_win32_write_8k(const fs::path &path, int iters) {
    auto wpath = path.wstring();
    IStorage *pStg = nullptr;
    StgCreateDocfile(wpath.c_str(), STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, &pStg);
    IStream *pStrm = nullptr;
    pStg->CreateStream(L"data", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &pStrm);
    std::vector<uint8_t> buf(8192, 0xCD);
    LARGE_INTEGER zero{};
    zero.QuadPart = 0;
    // Warmup
    for (int i = 0; i < 10; ++i) {
        pStrm->Seek(zero, STREAM_SEEK_SET, nullptr);
        ULONG w = 0;
        pStrm->Write(buf.data(), static_cast<ULONG>(buf.size()), &w);
    }
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iters; ++i) {
        pStrm->Seek(zero, STREAM_SEEK_SET, nullptr);
        ULONG written = 0;
        pStrm->Write(buf.data(), static_cast<ULONG>(buf.size()), &written);
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    if (pStrm) pStrm->Release();
    if (pStg) pStg->Release();
    return std::chrono::duration<double, std::micro>(t1 - t0).count() / iters;
}

static double bench_win32_read_1k(const fs::path &path, int iters) {
    auto wpath = path.wstring();
    IStorage *pStg = nullptr;
    StgCreateDocfile(wpath.c_str(), STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, &pStg);
    IStream *pStrm = nullptr;
    pStg->CreateStream(L"data", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &pStrm);
    std::vector<uint8_t> wbuf(1024, 0xAB);
    ULONG written = 0;
    pStrm->Write(wbuf.data(), static_cast<ULONG>(wbuf.size()), &written);
    std::vector<uint8_t> rbuf(1024);
    LARGE_INTEGER zero{};
    zero.QuadPart = 0;
    // Warmup
    for (int i = 0; i < 10; ++i) {
        pStrm->Seek(zero, STREAM_SEEK_SET, nullptr);
        ULONG r = 0;
        pStrm->Read(rbuf.data(), static_cast<ULONG>(rbuf.size()), &r);
    }
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iters; ++i) {
        pStrm->Seek(zero, STREAM_SEEK_SET, nullptr);
        ULONG read_bytes = 0;
        pStrm->Read(rbuf.data(), static_cast<ULONG>(rbuf.size()), &read_bytes);
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    if (pStrm) pStrm->Release();
    if (pStg) pStg->Release();
    return std::chrono::duration<double, std::micro>(t1 - t0).count() / iters;
}

static double bench_win32_read_8k(const fs::path &path, int iters) {
    auto wpath = path.wstring();
    IStorage *pStg = nullptr;
    StgCreateDocfile(wpath.c_str(), STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, &pStg);
    IStream *pStrm = nullptr;
    pStg->CreateStream(L"data", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &pStrm);
    std::vector<uint8_t> wbuf(8192, 0xCD);
    ULONG written = 0;
    pStrm->Write(wbuf.data(), static_cast<ULONG>(wbuf.size()), &written);
    std::vector<uint8_t> rbuf(8192);
    LARGE_INTEGER zero{};
    zero.QuadPart = 0;
    // Warmup
    for (int i = 0; i < 10; ++i) {
        pStrm->Seek(zero, STREAM_SEEK_SET, nullptr);
        ULONG r = 0;
        pStrm->Read(rbuf.data(), static_cast<ULONG>(rbuf.size()), &r);
    }
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iters; ++i) {
        pStrm->Seek(zero, STREAM_SEEK_SET, nullptr);
        ULONG read_bytes = 0;
        pStrm->Read(rbuf.data(), static_cast<ULONG>(rbuf.size()), &read_bytes);
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    if (pStrm) pStrm->Release();
    if (pStg) pStg->Release();
    return std::chrono::duration<double, std::micro>(t1 - t0).count() / iters;
}

static double bench_win32_enumerate(const fs::path &path, int iters) {
    auto wpath = path.wstring();
    IStorage *pStg = nullptr;
    StgCreateDocfile(wpath.c_str(), STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, &pStg);
    for (int i = 0; i < 20; ++i) {
        wchar_t name[32];
        swprintf_s(name, L"child_%d", i);
        IStream *pStrm = nullptr;
        pStg->CreateStream(name, STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &pStrm);
        if (pStrm) pStrm->Release();
    }
    // Warmup
    for (int i = 0; i < 5; ++i) {
        IEnumSTATSTG *pEnum = nullptr;
        pStg->EnumElements(0, nullptr, 0, &pEnum);
        if (pEnum) {
            STATSTG stat;
            while (pEnum->Next(1, &stat, nullptr) == S_OK) CoTaskMemFree(stat.pwcsName);
            pEnum->Release();
        }
    }
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iters; ++i) {
        IEnumSTATSTG *pEnum = nullptr;
        pStg->EnumElements(0, nullptr, 0, &pEnum);
        if (pEnum) {
            STATSTG stat;
            while (pEnum->Next(1, &stat, nullptr) == S_OK) {
                CoTaskMemFree(stat.pwcsName);
            }
            pEnum->Release();
        }
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    if (pStg) pStg->Release();
    return std::chrono::duration<double, std::micro>(t1 - t0).count() / iters;
}

// ── Run all benchmarks and display ───────────────────────────────────────

static void show_performance(console &con) {
    section(con, " Real Performance Benchmarks (stout vs Win32) ");

    auto tmp_dir = fs::temp_directory_path();
    auto stout_path = tmp_dir / "bench_stout.cfb";
    auto win32_path = tmp_dir / "bench_win32.cfb";

    con.print("[dim]Running benchmarks... (this may take a few seconds)[/]");
    con.newline();

    constexpr int N_CREATE = 200;
    constexpr int N_STREAM = 500;
    constexpr int N_IO = 5000;
    constexpr int N_ENUM = 2000;
    constexpr int N_PROP = 500;

    std::vector<bench_result> results;

    results.push_back(
        {"Create empty file", bench_stout_create(stout_path, N_CREATE), bench_win32_create(win32_path, N_CREATE)});

    results.push_back({"Create stream", bench_stout_create_stream(stout_path, N_STREAM),
                       bench_win32_create_stream(win32_path, N_STREAM)});

    results.push_back(
        {"Write 1KB (mini)", bench_stout_write_1k(stout_path, N_IO), bench_win32_write_1k(win32_path, N_IO)});

    results.push_back(
        {"Write 8KB (regular)", bench_stout_write_8k(stout_path, N_IO), bench_win32_write_8k(win32_path, N_IO)});

    results.push_back(
        {"Read 1KB (mini)", bench_stout_read_1k(stout_path, N_IO), bench_win32_read_1k(win32_path, N_IO)});

    results.push_back(
        {"Read 8KB (regular)", bench_stout_read_8k(stout_path, N_IO), bench_win32_read_8k(win32_path, N_IO)});

    results.push_back({"Enumerate 20 children", bench_stout_enumerate(stout_path, N_ENUM),
                       bench_win32_enumerate(win32_path, N_ENUM)});

    results.push_back({"Write property set", bench_stout_property_set(stout_path, N_PROP),
                       0.0}); // Win32 property set benchmark omitted (complex COM setup)

    // Cleanup temp files
    fs::remove(stout_path);
    fs::remove(win32_path);

    // Display results table
    table_builder tb;
    tb.add_column("Operation", {justify::left, 22, 28});
    tb.add_column("stout (us)", {justify::right, 10, 14});
    tb.add_column("Win32 (us)", {justify::right, 10, 14});
    tb.add_column("Ratio", {justify::right, 12, 16});
    tb.border(border_style::rounded);
    tb.header_style(style{colors::bright_yellow, {}, attr::bold});

    for (auto &r : results) {
        char s_buf[16], w_buf[16], ratio_buf[32];
        std::snprintf(s_buf, sizeof(s_buf), "%.1f", r.stout_us);

        if (r.win32_us > 0.0) {
            std::snprintf(w_buf, sizeof(w_buf), "%.1f", r.win32_us);
            double ratio = r.stout_us / r.win32_us;
            if (ratio <= 1.0)
                std::snprintf(ratio_buf, sizeof(ratio_buf), "[green]%.2fx faster[/]", 1.0 / ratio);
            else if (ratio <= 1.5)
                std::snprintf(ratio_buf, sizeof(ratio_buf), "[yellow]%.2fx slower[/]", ratio);
            else
                std::snprintf(ratio_buf, sizeof(ratio_buf), "[red]%.2fx slower[/]", ratio);
        } else {
            std::snprintf(w_buf, sizeof(w_buf), "N/A");
            std::snprintf(ratio_buf, sizeof(ratio_buf), "[dim]—[/]");
        }

        tb.add_row({r.name, s_buf, w_buf, ratio_buf});
    }
    con.print_widget(tb, W);

    // Bar chart: side-by-side comparison
    con.newline();
    con.print("[dim]Latency comparison (us, lower = better):[/]");
    con.print("[dim]  Green = stout, Cyan = Win32[/]");
    con.newline();

    // Interleaved bar chart: stout, win32, stout, win32, ...
    std::vector<float> bar_data;
    std::vector<std::string> bar_labels;
    for (auto &r : results) {
        if (r.win32_us <= 0.0) continue;
        bar_data.push_back(static_cast<float>(r.stout_us));
        bar_data.push_back(static_cast<float>(r.win32_us));
        // Short label
        std::string label(r.name);
        auto sp = label.find(' ');
        if (sp != std::string::npos) label = label.substr(0, sp);
        if (label.size() > 5) label = label.substr(0, 5);
        bar_labels.push_back(label + "S");
        bar_labels.push_back(label + "W");
    }
    con.print_widget(bar_chart_builder(bar_data, 7).labels(bar_labels).style_override(style{colors::bright_green}), W);
}

// ═══════════════════════════════════════════════════════════════════════
//  Code Comparison (API usage examples)
// ═══════════════════════════════════════════════════════════════════════

static void show_code_comparison(console &con) {
    section(con, " API Usage Comparison ");

    con.print("[bold]Create a compound file and write data:[/]");
    con.newline();

    columns_builder cols;

    // stout side
    {
        rows_builder code;
        code.add(text_builder("[bold green]stout (C++23)[/]"));
        code.add(text_builder("[dim]// 4 lines, zero boilerplate[/]"));
        code.add(text_builder("auto cf = compound_file::create(p);"));
        code.add(text_builder("auto root = cf->root_storage();"));
        code.add(text_builder("auto s = root.create_stream(\"data\");"));
        code.add(text_builder("s->write(0, buf);"));
        code.gap(0);

        panel_builder lp(std::move(code));
        lp.title("stout");
        lp.border(border_style::rounded);
        lp.border_style_override(style{colors::bright_green});
        cols.add(std::move(lp), 1);
    }

    // Win32 side
    {
        rows_builder code;
        code.add(text_builder("[bold cyan]Win32 COM[/]"));
        code.add(text_builder("[dim]// 12+ lines, COM boilerplate[/]"));
        code.add(text_builder("CoInitializeEx(0, COINIT_MT);"));
        code.add(text_builder("IStorage* pStg = nullptr;"));
        code.add(text_builder("StgCreateDocfile(wpath,"));
        code.add(text_builder("  STGM_CREATE|STGM_RW|STGM_SE,"));
        code.add(text_builder("  0, &pStg);"));
        code.add(text_builder("IStream* pStrm = nullptr;"));
        code.add(text_builder("pStg->CreateStream(L\"data\","));
        code.add(text_builder("  STGM_CREATE|STGM_RW|STGM_SE,"));
        code.add(text_builder("  0, 0, &pStrm);"));
        code.add(text_builder("ULONG written;"));
        code.add(text_builder("pStrm->Write(buf, sz, &written);"));
        code.add(text_builder("pStrm->Release();"));
        code.add(text_builder("pStg->Release();"));
        code.add(text_builder("CoUninitialize();"));
        code.gap(0);

        panel_builder rp(std::move(code));
        rp.title("Win32");
        rp.border(border_style::rounded);
        rp.border_style_override(style{colors::bright_cyan});
        cols.add(std::move(rp), 1);
    }

    cols.gap(1);
    con.print_widget(std::move(cols), W);

    // Lines of code comparison
    con.newline();
    con.print("[dim]Lines of code for common operations:[/]");

    table_builder tb;
    tb.add_column("Operation", {justify::left, 24, 30});
    tb.add_column("stout (lines)", {justify::right, 12, 16});
    tb.add_column("Win32 (lines)", {justify::right, 12, 16});
    tb.add_column("Reduction", {justify::right, 12, 16});
    tb.border(border_style::rounded);
    tb.header_style(style{colors::bright_yellow, {}, attr::bold});

    struct loc_entry {
        const char *name;
        int stout;
        int win32;
    };
    loc_entry locs[] = {
        {"Create + write stream", 4, 16}, {"Read stream data", 3, 12}, {"Enumerate children", 1, 8},
        {"Rename entry", 1, 3},           {"Delete entry", 1, 3},      {"Set timestamps", 1, 5},
        {"Property set write", 3, 25},    {"Error handling", 1, 6},
    };

    for (auto &e : locs) {
        char s[8], w[8], r[32];
        std::snprintf(s, sizeof(s), "%d", e.stout);
        std::snprintf(w, sizeof(w), "%d", e.win32);
        double pct = (1.0 - static_cast<double>(e.stout) / e.win32) * 100.0;
        std::snprintf(r, sizeof(r), "[green]-%.0f%%[/]", pct);
        tb.add_row({e.name, s, w, r});
    }
    con.print_widget(tb, W);
}

// ═══════════════════════════════════════════════════════════════════════
//  Architecture Comparison
// ═══════════════════════════════════════════════════════════════════════

static void show_architecture(console &con) {
    section(con, " Architecture Comparison ");

    columns_builder cols;

    // stout architecture
    {
        rows_builder layers;
        layers.add(text_builder("[bold bright_white on_green]  compound_file (C++23)  [/]"));
        layers.add(text_builder("[dim]          |[/]"));
        layers.add(text_builder("[bold bright_white on_green]  storage / stream       [/]"));
        layers.add(text_builder("[dim]          |[/]"));
        layers.add(text_builder("[bold bright_white on_green]  FAT / mini-FAT         [/]"));
        layers.add(text_builder("[dim]          |[/]"));
        layers.add(text_builder("[bold bright_white on_green]  sector I/O layer       [/]"));
        layers.add(text_builder("[dim]          |[/]"));
        layers.add(text_builder("[bold bright_white on_green]  file / memory backend  [/]"));
        layers.gap(0);

        panel_builder lp(std::move(layers));
        lp.title("stout Layers");
        lp.border(border_style::rounded);
        lp.border_style_override(style{colors::bright_green});
        cols.add(std::move(lp), 1);
    }

    // Win32 architecture
    {
        rows_builder layers;
        layers.add(text_builder("[bold bright_white on_cyan]  IStorage / IStream COM [/]"));
        layers.add(text_builder("[dim]          |[/]"));
        layers.add(text_builder("[bold bright_white on_cyan]  ole32.dll (opaque)     [/]"));
        layers.add(text_builder("[dim]          |[/]"));
        layers.add(text_builder("[bold bright_white on_cyan]  ILockBytes (optional)  [/]"));
        layers.add(text_builder("[dim]          |[/]"));
        layers.add(text_builder("[bold bright_white on_cyan]  Windows kernel I/O     [/]"));
        layers.add(text_builder("[dim]          |[/]"));
        layers.add(text_builder("[bold bright_white on_cyan]  NTFS / disk            [/]"));
        layers.gap(0);

        panel_builder rp(std::move(layers));
        rp.title("Win32 Layers");
        rp.border(border_style::rounded);
        rp.border_style_override(style{colors::bright_cyan});
        cols.add(std::move(rp), 1);
    }

    cols.gap(1);
    con.print_widget(std::move(cols), W);
}

// ═══════════════════════════════════════════════════════════════════════
//  Test Coverage
// ═══════════════════════════════════════════════════════════════════════

static void show_test_coverage(console &con) {
    section(con, " Win32 Conformance Test Coverage ");

    con.print("[dim]stout passes 1153 tests including 924 Win32 cross-validation tests[/]");
    con.newline();

    columns_builder cols;

    {
        rows_builder gauges;
        gauges.add(text_builder("[bold]Win32 Conformance[/]  924/924"));
        gauges.add(make_gauge(1.0f));
        gauges.add(text_builder("[bold]Unit Tests[/]         229/229"));
        gauges.add(make_gauge(1.0f));
        gauges.add(text_builder("[bold]Total (ctest)[/]      1153/1153"));
        gauges.add(make_gauge(1.0f));
        gauges.gap(0);

        panel_builder gp(std::move(gauges));
        gp.title("Pass Rate");
        gp.border(border_style::rounded);
        cols.add(std::move(gp), 1);
    }

    {
        table_builder tb;
        tb.add_column("Test Category", {justify::left, 22, 28});
        tb.add_column("Count", {justify::right, 6, 10});
        tb.border(border_style::rounded);
        tb.header_style(style{colors::bright_cyan, {}, attr::bold});

        tb.add_row({"Basic conformance", "97"});
        tb.add_row({"Stress: read/write", "56"});
        tb.add_row({"Stress: hierarchy", "48"});
        tb.add_row({"Stress: property set", "42"});
        tb.add_row({"Stress: cross-API", "38"});
        tb.add_row({"Stress: data patterns", "44"});
        tb.add_row({"Stress: roundtrip", "36"});
        tb.add_row({"Stress: exists/stat", "52"});
        tb.add_row({"Other stress tests", "511"});
        tb.add_row({"[bold]Total[/]", "[bold]924[/]"});

        cols.add(std::move(tb), 1);
    }

    cols.gap(1);
    con.print_widget(std::move(cols), W);
}

// ═══════════════════════════════════════════════════════════════════════
//  Advantages Summary
// ═══════════════════════════════════════════════════════════════════════

static void show_advantages(console &con) {
    section(con, " Summary: stout Advantages ");

    columns_builder cols;

    {
        rows_builder left;
        left.add(text_builder("[bold green]stout Wins[/]"));
        left.add(text_builder("  [green]+[/] Cross-platform (Win/Linux/Mac)"));
        left.add(text_builder("  [green]+[/] Modern C++23 API"));
        left.add(text_builder("  [green]+[/] std::expected error handling"));
        left.add(text_builder("  [green]+[/] RAII — no manual Release()"));
        left.add(text_builder("  [green]+[/] No COM initialization needed"));
        left.add(text_builder("  [green]+[/] In-memory file support"));
        left.add(text_builder("  [green]+[/] 60-80% less code to write"));
        left.add(text_builder("  [green]+[/] UTF-8 string_view API"));
        left.add(text_builder("  [green]+[/] Header-only friendly"));
        left.gap(0);

        panel_builder lp(std::move(left));
        lp.border(border_style::rounded);
        lp.border_style_override(style{colors::bright_green});
        cols.add(std::move(lp), 1);
    }

    {
        rows_builder right;
        right.add(text_builder("[bold cyan]Win32 Wins[/]"));
        right.add(text_builder("  [cyan]+[/] Kernel-level I/O optimization"));
        right.add(text_builder("  [cyan]+[/] Decades of battle-testing"));
        right.add(text_builder("  [cyan]+[/] COM interop ecosystem"));
        right.add(text_builder("  [cyan]+[/] Structured Storage Viewer"));
        right.add(text_builder("  [cyan]+[/] ILockBytes extensibility"));
        right.add(text_builder("  [cyan]+[/] System-wide file locking"));
        right.add(text_builder(""));
        right.add(text_builder(""));
        right.add(text_builder(""));
        right.gap(0);

        panel_builder rp(std::move(right));
        rp.border(border_style::rounded);
        rp.border_style_override(style{colors::bright_cyan});
        cols.add(std::move(rp), 1);
    }

    cols.gap(1);
    con.print_widget(std::move(cols), W);
}

// ═══════════════════════════════════════════════════════════════════════
//  Main
// ═══════════════════════════════════════════════════════════════════════

int main() {
    com_guard com;
    console con;

    show_header(con);
    show_api_comparison(con);
    show_feature_matrix(con);
    show_performance(con);
    show_code_comparison(con);
    show_architecture(con);
    show_test_coverage(con);
    show_advantages(con);

    con.newline();
    con.print_widget(status_bar_builder()
                         .left("[bold] stout vs Win32 [/]")
                         .center("Compound File Binary Library")
                         .right("33 features | 1153 tests")
                         .style_override(style{colors::bright_white, color::from_rgb(30, 50, 30)}),
                     W);
    con.newline();
    con.print_widget(rule_builder(" stout Dashboard Complete ")
                         .rule_style(style{colors::bright_green, {}, attr::bold})
                         .character(U'\x2550'),
                     W);
    con.newline();

    return 0;
}
