#pragma once

#ifdef _WIN32

#    include "win32_helpers.h"

#    include <cstdint>
#    include <filesystem>
#    include <fstream>
#    include <optional>
#    include <random>
#    include <string>
#    include <vector>

namespace conformance {

    namespace fs = std::filesystem;

    // Creates a pair of temp file paths (one for Stout, one for Win32)
    inline auto temp_file_pair(std::string_view prefix) -> std::pair<fs::path, fs::path> {
        auto tmp = fs::temp_directory_path();
        // Use a random suffix to avoid collisions
        std::random_device rd;
        auto suffix = std::to_string(rd());
        auto stout_path = tmp / (std::string(prefix) + "_stout_" + suffix + ".cfb");
        auto win32_path = tmp / (std::string(prefix) + "_win32_" + suffix + ".cfb");
        // Clean up any leftover files
        std::error_code ec;
        fs::remove(stout_path, ec);
        fs::remove(win32_path, ec);
        return {stout_path, win32_path};
    }

    // Creates a single temp file path
    inline auto temp_file(std::string_view prefix) -> fs::path {
        auto tmp = fs::temp_directory_path();
        std::random_device rd;
        auto suffix = std::to_string(rd());
        auto path = tmp / (std::string(prefix) + "_" + suffix + ".cfb");
        std::error_code ec;
        fs::remove(path, ec);
        return path;
    }

    // RAII temp file cleanup
    struct temp_file_guard {
        std::vector<fs::path> paths;
        ~temp_file_guard() {
            std::error_code ec;
            for (auto &p : paths) {
                fs::remove(p, ec);
            }
        }
        void add(const fs::path &p) { paths.push_back(p); }
    };

    // Read entire file into a byte vector
    inline auto read_file_bytes(const fs::path &path) -> std::vector<uint8_t> {
        std::ifstream f(path, std::ios::binary | std::ios::ate);
        if (!f) {
            return {};
        }
        auto sz = f.tellg();
        f.seekg(0);
        std::vector<uint8_t> buf(static_cast<size_t>(sz));
        f.read(reinterpret_cast<char *>(buf.data()), sz);
        return buf;
    }

    // Byte-compare two files, returns mismatch offset or nullopt if identical
    inline auto compare_files(const fs::path &a, const fs::path &b) -> std::optional<uint64_t> {
        auto da = read_file_bytes(a);
        auto db = read_file_bytes(b);
        auto min_sz = std::min(da.size(), db.size());
        for (size_t i = 0; i < min_sz; ++i) {
            if (da[i] != db[i]) {
                return static_cast<uint64_t>(i);
            }
        }
        if (da.size() != db.size()) {
            return static_cast<uint64_t>(min_sz);
        }
        return std::nullopt;
    }

    // Generate a test data pattern of given size
    inline auto make_test_data(size_t size, uint8_t seed = 0) -> std::vector<uint8_t> {
        std::vector<uint8_t> data(size);
        for (size_t i = 0; i < size; ++i) {
            data[i] = static_cast<uint8_t>((i + seed) & 0xFF);
        }
        return data;
    }

    // Write data to a Win32 IStream
    inline HRESULT win32_stream_write(IStream *strm, const void *data, ULONG size) {
        ULONG written = 0;
        HRESULT hr = strm->Write(data, size, &written);
        if (SUCCEEDED(hr) && written != size) {
            return E_FAIL;
        }
        return hr;
    }

    // Read data from a Win32 IStream
    inline HRESULT win32_stream_read(IStream *strm, void *buf, ULONG size, ULONG *actual = nullptr) {
        ULONG read_count = 0;
        HRESULT hr = strm->Read(buf, size, &read_count);
        if (actual) {
            *actual = read_count;
        }
        return hr;
    }

    // Get IStream size via Stat
    inline auto win32_stream_size(IStream *strm) -> uint64_t {
        STATSTG st{};
        strm->Stat(&st, STATFLAG_NONAME);
        return st.cbSize.QuadPart;
    }

    // Enumerate children of an IStorage, returns vector of STATSTG
    inline auto win32_enumerate(IStorage *stg) -> std::vector<STATSTG> {
        std::vector<STATSTG> result;
        enum_stat_ptr enumerator;
        if (FAILED(stg->EnumElements(0, nullptr, 0, enumerator.put()))) {
            return result;
        }
        STATSTG st{};
        while (enumerator->Next(1, &st, nullptr) == S_OK) {
            result.push_back(st);
            // Don't free pwcsName here — caller must handle it
        }
        return result;
    }

    // Free STATSTG name
    inline void free_statstg_name(STATSTG &st) {
        if (st.pwcsName) {
            CoTaskMemFree(st.pwcsName);
            st.pwcsName = nullptr;
        }
    }

} // namespace conformance

#endif // _WIN32
