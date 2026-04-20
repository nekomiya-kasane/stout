# stout

A cross-platform C++23 library for reading and writing Microsoft Compound File Binary (CFB) format files — the underlying storage format for `.doc`, `.xls`, `.ppt`, `.msi`, `.msg`, and other OLE structured storage documents.

## Features

- **Full CFB v3/v4 support** — 512-byte and 4096-byte sector sizes
- **Read & write** — open existing files or create new ones
- **Storages & streams** — hierarchical directory with nested storages and data streams
- **Mini streams** — automatic handling of streams smaller than the 4096-byte cutoff
- **OLE property sets** — parse and serialize `\005SummaryInformation`, `\005DocumentSummaryInformation`, and custom property sets
- **In-memory files** — create and manipulate compound files entirely in RAM
- **Transactions** — begin/commit/rollback with snapshot isolation
- **Cross-platform** — no Win32 dependencies in the core library; builds on Windows, Linux, macOS, and Emscripten
- **Shared library** — builds as a DLL/SO with clean C++ API and `STOUT_API` export macros

## Quick Start

```cpp
#include <stout/compound_file.h>
#include <print>

int main() {
    // Create a new compound file
    auto cf = stout::compound_file::create("example.cfb", stout::cfb_version::v4);
    auto root = cf->root_storage();

    // Create a storage (folder) and a stream (file)
    auto docs = root.create_storage("Documents");
    auto readme = docs->create_stream("readme.txt");

    // Write data
    std::string text = "Hello from stout!";
    readme->write(0, std::as_bytes(std::span(text)));
    cf->flush();
}
```

```cpp
// Open and read an existing .doc / .xls / .cfb file
auto cf = stout::compound_file::open("document.doc", stout::open_mode::read);
auto root = cf->root_storage();

for (auto& entry : root.children()) {
    std::println("{} [{}] {} bytes", entry.name,
        entry.type == stout::entry_type::storage ? "storage" : "stream",
        entry.size);
}

auto stream = root.open_stream("WordDocument");
std::vector<uint8_t> buf(stream->size());
stream->read(0, std::span<uint8_t>(buf));
```

## Building

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### Options

| Option | Default | Description |
|---|---|---|
| `STOUT_BUILD_TESTS` | `ON` | Build the test suite (1153 tests) |
| `STOUT_BUILD_DEMOS` | `ON` | Build demo executables |
| `STOUT_BUILD_DASHBOARD` | `OFF` | Build tapiru-based benchmark dashboard (requires tapiru) |

### Requirements

- **C++23** compiler (MSVC 17.8+, GCC 14+, Clang 18+)
- **CMake** 3.22+
- No external dependencies for the core library

## Architecture

```
stout/
├── include/stout/
│   ├── compound_file.h        ← Public API: compound_file, storage, stream
│   ├── types.h                ← error, open_mode, cfb_version, entry_stat, file_time
│   ├── stout.h                ← Convenience umbrella header
│   ├── version.h              ← Library version info
│   ├── exports.h              ← STOUT_API DLL export macros
│   ├── cfb/                   ← Low-level CFB format internals
│   │   ├── header.h           ← 512-byte CFB header parse/serialize
│   │   ├── fat.h              ← File Allocation Table (sector chains)
│   │   ├── difat.h            ← Double-Indirect FAT (for large files)
│   │   ├── mini_fat.h         ← Mini-stream FAT (for small streams)
│   │   ├── directory.h        ← Directory entry red-black tree
│   │   ├── sector_io.h        ← Sector-level read/write abstraction
│   │   └── constants.h        ← Magic numbers, sector IDs, sizes
│   ├── io/                    ← I/O backends
│   │   ├── lock_bytes.h       ← Abstract byte-range I/O interface
│   │   ├── file_lock_bytes.h  ← File-backed I/O with write-back page cache
│   │   └── memory_lock_bytes.h← In-memory I/O backend
│   ├── ole/                   ← OLE property set support
│   │   ├── property_set.h     ← VT types, property_value, parse/serialize
│   │   └── property_set_storage.h ← Read/write property sets from storages
│   └── util/                  ← Utilities
│       ├── guid.h             ← GUID type and formatting
│       ├── filetime.h         ← Windows FILETIME ↔ system_clock conversion
│       ├── unicode.h          ← UTF-8 ↔ UTF-16LE conversion
│       └── endian.h           ← Little-endian byte swapping
├── src/                       ← Implementation files
├── tests/                     ← 1153 unit tests (Google Test)
├── demos/                     ← Example programs
│   ├── demo_compound_file.cpp ← Create/read/write demo
│   ├── stout_dashboard.cpp    ← Benchmark dashboard (stout vs Win32)
│   └── ss_viewer.cpp          ← Interactive TUI structured storage viewer
└── testdata/                  ← Sample CFB files for testing
```

### Layered Design

```
┌─────────────────────────────────────────────────┐
│  Public API                                     │
│  compound_file · storage · stream               │
│  ole::property_set · ole::property_set_storage   │
├─────────────────────────────────────────────────┤
│  CFB Engine                                     │
│  header · fat · difat · mini_fat · directory     │
│  sector_io · mini_stream_io                      │
├─────────────────────────────────────────────────┤
│  I/O Backends                                   │
│  file_lock_bytes (page cache) │ memory_lock_bytes│
└─────────────────────────────────────────────────┘
```

**Public API** — `compound_file` is the entry point. Call `open()` or `create()` to get a file handle, then `root_storage()` to navigate the hierarchy. `storage` provides `children()`, `create_stream()`, `create_storage()`, `open_stream()`, `open_storage()`, and metadata access (`stat()`, `clsid()`, timestamps). `stream` provides `read()`, `write()`, `size()`, `resize()`.

**CFB Engine** — Implements the [MS-CFB] specification. The FAT maps sector chains, the DIFAT extends the FAT for large files, the mini-FAT handles streams below the 4096-byte cutoff, and the directory stores a red-black tree of named entries.

**I/O Backends** — `file_lock_bytes` provides file-backed I/O with a write-back page cache (4KB pages, sorted+coalesced flush). `memory_lock_bytes` provides a `std::vector<uint8_t>`-backed backend for in-memory files.

### Key Design Decisions

- **`std::expected<T, error>`** for all fallible operations — no exceptions
- **Move-only types** — `compound_file`, `storage`, `stream` are non-copyable
- **UTF-8 API** — all string parameters are UTF-8; internal conversion to UTF-16LE for CFB directory entries
- **Write-back page cache** — `file_lock_bytes` buffers dirty 4KB pages and flushes them sorted by page ID with contiguous coalescing for optimal sequential I/O
- **Transaction support** — `begin_transaction()` snapshots all metadata; `commit()` applies changes; `rollback()` restores the snapshot

## API Reference

### `compound_file`

| Method | Description |
|---|---|
| `open(path, mode)` | Open an existing CFB file (read or read_write) |
| `create(path, version)` | Create a new CFB file (v3 or v4) |
| `create_in_memory(version)` | Create a new in-memory CFB file |
| `open_from_memory(data)` | Open a CFB file from a byte vector |
| `root_storage()` | Get the root storage entry |
| `flush()` | Write all pending changes to disk |
| `version()` | Get the CFB version (v3 or v4) |
| `data()` | Get raw bytes (in-memory files only) |
| `begin_transaction()` | Start a transaction |
| `commit()` | Commit the current transaction |
| `rollback()` | Rollback the current transaction |

### `storage`

| Method | Description |
|---|---|
| `children()` | List all child entries (returns `vector<entry_stat>`) |
| `create_stream(name)` | Create a new stream |
| `create_storage(name)` | Create a new sub-storage |
| `open_stream(name)` | Open an existing stream |
| `open_storage(name)` | Open an existing sub-storage |
| `remove(name)` | Remove a child entry |
| `rename(new_name)` | Rename this storage |
| `stat()` | Get metadata (`entry_stat`) |
| `clsid()` / `set_clsid()` | Get/set the storage CLSID |
| `state_bits()` / `set_state_bits()` | Get/set state bits |
| `set_creation_time()` / `set_modified_time()` | Set timestamps |

### `stream`

| Method | Description |
|---|---|
| `read(offset, buffer)` | Read bytes from the stream |
| `write(offset, data)` | Write bytes to the stream |
| `size()` | Get stream size in bytes |
| `resize(new_size)` | Resize (triggers mini↔regular migration at 4096 bytes) |

### OLE Property Sets

```cpp
#include <stout/ole/property_set_storage.h>

// Read SummaryInformation from a .doc file
auto cf = stout::compound_file::open("document.doc");
auto root = cf->root_storage();
auto ps = stout::ole::read_summary_info(root);
auto* sec = ps->section(stout::ole::fmtid_summary_information());
std::println("Title:  {}", sec->get_string(stout::ole::pid::title));
std::println("Author: {}", sec->get_string(stout::ole::pid::author));
```

## Testing

```bash
cd build
ctest --build-config Release -j8
# 1153/1153 tests pass
```

Tests cover: header parse/serialize, FAT chain operations, DIFAT, mini-FAT, directory red-black tree, sector I/O, file and memory backends, compound file create/open/read/write, stream resize and mini↔regular migration, storage operations, transactions, OLE property sets, Unicode conversion, GUID handling, endianness, and Win32 conformance.

## Demos

### Structured Storage Viewer (`ss_viewer`)

Interactive fullscreen TUI for browsing CFB files, built on the [tapiru](../tapiru) TUI framework.

```bash
cmake -B build -DSTOUT_BUILD_DASHBOARD=ON
cmake --build build --target ss_viewer --config Release

# Open with stout backend
ss_viewer.exe testdata/sample_word.doc

# Open with Win32 IStorage backend
ss_viewer.exe --win32 testdata/sample_word.doc
```

Features: tree view of storage hierarchy, entry metadata inspector, hex dump viewer, OLE property set parser.

### Benchmark Dashboard (`stout_dashboard`)

Side-by-side performance comparison of stout vs Win32 Structured Storage API.

```bash
cmake --build build --target stout_dashboard --config Release
stout_dashboard.exe
```

## License

See the project license file for details.
