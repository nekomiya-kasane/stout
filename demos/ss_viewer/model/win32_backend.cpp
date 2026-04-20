/**
 * @file win32_backend.cpp
 * @brief Win32 IStorage backend implementation.
 */
#include "ss_viewer/model/win32_backend.h"

#include <algorithm>
#include <cstring>
#include <memory>

namespace ssv {

/// Helper: navigate to a Win32 IStorage by full_path. Caller must Release() the result.
static IStorage *navigate_to_win32_storage(IStorage *root_stg, const std::string &full_path) {
    std::vector<std::string> parts;
    {
        std::string s = full_path;
        size_t pos = 0;
        while ((pos = s.find('/')) != std::string::npos) {
            parts.push_back(s.substr(0, pos));
            s.erase(0, pos + 1);
        }
        parts.push_back(s);
    }
    IStorage *cur = root_stg;
    cur->AddRef();
    for (size_t i = 1; i < parts.size(); ++i) {
        int needed = MultiByteToWideChar(CP_UTF8, 0, parts[i].c_str(), -1, nullptr, 0);
        std::wstring wname(needed - 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, parts[i].c_str(), -1, wname.data(), needed);
        IStorage *next = nullptr;
        if (FAILED(cur->OpenStorage(wname.c_str(), nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0, &next))) {
            cur->Release();
            return nullptr;
        }
        cur->Release();
        cur = next;
    }
    return cur;
}

/// Helper: convert wide string to UTF-8.
static std::string wide_to_utf8(const std::wstring &wstr) {
    int needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string u8(needed - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, u8.data(), needed, nullptr, nullptr);
    return u8;
}

entry_info build_win32_tree(IStorage *stg, const std::string &parent_path, const std::string &name) {
    entry_info info;
    info.name = name;
    info.type = stout::entry_type::storage;
    info.full_path = parent_path.empty() ? name : parent_path + "/" + name;

    // Get stat
    STATSTG stat_stg;
    if (SUCCEEDED(stg->Stat(&stat_stg, STATFLAG_DEFAULT))) {
        info.size = stat_stg.cbSize.QuadPart;
        if (stat_stg.pwcsName) CoTaskMemFree(stat_stg.pwcsName);
        // CLSID
        std::memcpy(&info.clsid, &stat_stg.clsid, sizeof(stout::guid));
        // Timestamps
        ULARGE_INTEGER ct, mt;
        ct.LowPart = stat_stg.ctime.dwLowDateTime;
        ct.HighPart = stat_stg.ctime.dwHighDateTime;
        mt.LowPart = stat_stg.mtime.dwLowDateTime;
        mt.HighPart = stat_stg.mtime.dwHighDateTime;
        if (ct.QuadPart != 0) {
            auto ft_ns = ct.QuadPart * 100;
            auto epoch_diff = std::chrono::seconds(11644473600LL);
            info.creation_time = stout::file_time(std::chrono::duration_cast<std::chrono::system_clock::duration>(
                std::chrono::nanoseconds(ft_ns) - epoch_diff));
        }
        if (mt.QuadPart != 0) {
            auto ft_ns = mt.QuadPart * 100;
            auto epoch_diff = std::chrono::seconds(11644473600LL);
            info.modified_time = stout::file_time(std::chrono::duration_cast<std::chrono::system_clock::duration>(
                std::chrono::nanoseconds(ft_ns) - epoch_diff));
        }
        if (parent_path.empty()) info.type = stout::entry_type::root;
    }

    info.children_loaded = true;
    IEnumSTATSTG *pEnum = nullptr;
    if (SUCCEEDED(stg->EnumElements(0, nullptr, 0, &pEnum)) && pEnum) {
        STATSTG child_stat;
        while (pEnum->Next(1, &child_stat, nullptr) == S_OK) {
            std::wstring wname(child_stat.pwcsName);
            CoTaskMemFree(child_stat.pwcsName);
            auto u8name = wide_to_utf8(wname);

            if (child_stat.type == STGTY_STORAGE) {
                IStorage *child_stg = nullptr;
                if (SUCCEEDED(stg->OpenStorage(wname.c_str(), nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0,
                                               &child_stg))) {
                    info.children.push_back(build_win32_tree(child_stg, info.full_path, u8name));
                    child_stg->Release();
                }
            } else {
                entry_info ci;
                ci.name = u8name;
                ci.type = stout::entry_type::stream;
                ci.size = child_stat.cbSize.QuadPart;
                ci.full_path = info.full_path + "/" + u8name;
                ci.children_loaded = true;
                std::memcpy(&ci.clsid, &child_stat.clsid, sizeof(stout::guid));
                info.children.push_back(std::move(ci));
            }
        }
        pEnum->Release();
    }
    return info;
}

entry_info build_win32_tree_shallow(IStorage *stg, const std::string &parent_path, const std::string &name) {
    entry_info info;
    info.name = name;
    info.type = stout::entry_type::storage;
    info.full_path = parent_path.empty() ? name : parent_path + "/" + name;
    info.children_loaded = true; // first level loaded

    STATSTG stat_stg;
    if (SUCCEEDED(stg->Stat(&stat_stg, STATFLAG_DEFAULT))) {
        info.size = stat_stg.cbSize.QuadPart;
        if (stat_stg.pwcsName) CoTaskMemFree(stat_stg.pwcsName);
        std::memcpy(&info.clsid, &stat_stg.clsid, sizeof(stout::guid));
        ULARGE_INTEGER ct, mt;
        ct.LowPart = stat_stg.ctime.dwLowDateTime;
        ct.HighPart = stat_stg.ctime.dwHighDateTime;
        mt.LowPart = stat_stg.mtime.dwLowDateTime;
        mt.HighPart = stat_stg.mtime.dwHighDateTime;
        if (ct.QuadPart != 0) {
            auto ft_ns = ct.QuadPart * 100;
            auto epoch_diff = std::chrono::seconds(11644473600LL);
            info.creation_time = stout::file_time(std::chrono::duration_cast<std::chrono::system_clock::duration>(
                std::chrono::nanoseconds(ft_ns) - epoch_diff));
        }
        if (mt.QuadPart != 0) {
            auto ft_ns = mt.QuadPart * 100;
            auto epoch_diff = std::chrono::seconds(11644473600LL);
            info.modified_time = stout::file_time(std::chrono::duration_cast<std::chrono::system_clock::duration>(
                std::chrono::nanoseconds(ft_ns) - epoch_diff));
        }
        if (parent_path.empty()) info.type = stout::entry_type::root;
    }

    IEnumSTATSTG *pEnum = nullptr;
    if (SUCCEEDED(stg->EnumElements(0, nullptr, 0, &pEnum)) && pEnum) {
        STATSTG child_stat;
        while (pEnum->Next(1, &child_stat, nullptr) == S_OK) {
            std::wstring wname(child_stat.pwcsName);
            CoTaskMemFree(child_stat.pwcsName);
            auto u8name = wide_to_utf8(wname);

            entry_info ci;
            ci.name = u8name;
            ci.full_path = info.full_path + "/" + u8name;
            ci.size = child_stat.cbSize.QuadPart;
            std::memcpy(&ci.clsid, &child_stat.clsid, sizeof(stout::guid));

            if (child_stat.type == STGTY_STORAGE) {
                ci.type = stout::entry_type::storage;
                // children_loaded = false → lazy
            } else {
                ci.type = stout::entry_type::stream;
                ci.children_loaded = true;
            }
            info.children.push_back(std::move(ci));
        }
        pEnum->Release();
    }
    return info;
}

void load_win32_children(IStorage *root_stg, entry_info &ei) {
    if (ei.children_loaded) return;
    if (ei.type != stout::entry_type::storage && ei.type != stout::entry_type::root) {
        ei.children_loaded = true;
        return;
    }

    IStorage *stg = navigate_to_win32_storage(root_stg, ei.full_path);
    if (!stg) {
        ei.children_loaded = true;
        return;
    }

    ei.children.clear();
    IEnumSTATSTG *pEnum = nullptr;
    if (SUCCEEDED(stg->EnumElements(0, nullptr, 0, &pEnum)) && pEnum) {
        STATSTG child_stat;
        while (pEnum->Next(1, &child_stat, nullptr) == S_OK) {
            std::wstring wname(child_stat.pwcsName);
            CoTaskMemFree(child_stat.pwcsName);
            auto u8name = wide_to_utf8(wname);

            entry_info ci;
            ci.name = u8name;
            ci.full_path = ei.full_path + "/" + u8name;
            ci.size = child_stat.cbSize.QuadPart;
            std::memcpy(&ci.clsid, &child_stat.clsid, sizeof(stout::guid));

            if (child_stat.type == STGTY_STORAGE) {
                ci.type = stout::entry_type::storage;
            } else {
                ci.type = stout::entry_type::stream;
                ci.children_loaded = true;
            }
            ei.children.push_back(std::move(ci));
        }
        pEnum->Release();
    }
    stg->Release();
    ei.children_loaded = true;
}

std::vector<uint8_t> read_win32_stream(IStorage *root_stg, const entry_info &ei, uint64_t max_bytes) {
    // Navigate to the stream by path
    std::vector<std::string> parts;
    {
        std::string s = ei.full_path;
        size_t pos = 0;
        while ((pos = s.find('/')) != std::string::npos) {
            parts.push_back(s.substr(0, pos));
            s.erase(0, pos + 1);
        }
        parts.push_back(s);
    }

    // Navigate storages (skip root name)
    IStorage *cur = root_stg;
    cur->AddRef();
    for (size_t i = 1; i + 1 < parts.size(); ++i) {
        int needed = MultiByteToWideChar(CP_UTF8, 0, parts[i].c_str(), -1, nullptr, 0);
        std::wstring wname(needed - 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, parts[i].c_str(), -1, wname.data(), needed);
        IStorage *next = nullptr;
        if (FAILED(cur->OpenStorage(wname.c_str(), nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0, &next))) {
            cur->Release();
            return {};
        }
        cur->Release();
        cur = next;
    }

    // Open stream
    int needed = MultiByteToWideChar(CP_UTF8, 0, parts.back().c_str(), -1, nullptr, 0);
    std::wstring wname(needed - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, parts.back().c_str(), -1, wname.data(), needed);
    IStream *pStrm = nullptr;
    if (FAILED(cur->OpenStream(wname.c_str(), nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, &pStrm))) {
        cur->Release();
        return {};
    }
    cur->Release();

    uint64_t sz = std::min(ei.size, max_bytes);
    std::vector<uint8_t> buf(static_cast<size_t>(sz));
    if (sz > 0) {
        ULONG read = 0;
        pStrm->Read(buf.data(), static_cast<ULONG>(sz), &read);
        buf.resize(read);
    }
    pStrm->Release();
    return buf;
}

/// @brief RAII wrapper for IStream shared_ptr custom deleter.
struct istream_deleter {
    void operator()(IStream *p) const {
        if (p) p->Release();
    }
};

paged_reader open_win32_reader(IStorage *root_stg, const entry_info &ei) {
    if (ei.type != stout::entry_type::stream) return {};

    // Navigate to the stream by path
    std::vector<std::string> parts;
    {
        std::string s = ei.full_path;
        size_t pos = 0;
        while ((pos = s.find('/')) != std::string::npos) {
            parts.push_back(s.substr(0, pos));
            s.erase(0, pos + 1);
        }
        parts.push_back(s);
    }

    // Navigate storages (skip root name)
    IStorage *cur = root_stg;
    cur->AddRef();
    for (size_t i = 1; i + 1 < parts.size(); ++i) {
        int needed = MultiByteToWideChar(CP_UTF8, 0, parts[i].c_str(), -1, nullptr, 0);
        std::wstring wname(needed - 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, parts[i].c_str(), -1, wname.data(), needed);
        IStorage *next = nullptr;
        if (FAILED(cur->OpenStorage(wname.c_str(), nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0, &next))) {
            cur->Release();
            return {};
        }
        cur->Release();
        cur = next;
    }

    // Open stream
    int needed = MultiByteToWideChar(CP_UTF8, 0, parts.back().c_str(), -1, nullptr, 0);
    std::wstring wname(needed - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, parts.back().c_str(), -1, wname.data(), needed);
    IStream *pStrm = nullptr;
    if (FAILED(cur->OpenStream(wname.c_str(), nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, &pStrm))) {
        cur->Release();
        return {};
    }
    cur->Release();

    // Wrap in shared_ptr with custom deleter so the lambda can share ownership
    auto strm = std::shared_ptr<IStream>(pStrm, istream_deleter{});
    uint64_t sz = ei.size;

    return paged_reader(sz, [strm](uint64_t offset, std::span<uint8_t> buf) -> size_t {
        LARGE_INTEGER li;
        li.QuadPart = static_cast<LONGLONG>(offset);
        if (FAILED(strm->Seek(li, STREAM_SEEK_SET, nullptr))) return 0;
        ULONG read = 0;
        if (FAILED(strm->Read(buf.data(), static_cast<ULONG>(buf.size()), &read))) return 0;
        return static_cast<size_t>(read);
    });
}

} // namespace ssv
