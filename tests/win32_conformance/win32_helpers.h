#pragma once

// Thin RAII wrappers over Win32 COM Structured Storage APIs
// Only compiled on Windows

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <comdef.h>
#include <objbase.h>
#include <propidl.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <windows.h>

namespace conformance {

// RAII COM initializer
struct com_init {
    com_init() {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
            throw std::runtime_error("CoInitializeEx failed");
        }
    }
    ~com_init() { CoUninitialize(); }
    com_init(const com_init &) = delete;
    com_init &operator=(const com_init &) = delete;
};

// Simple Release-on-destruct wrapper for COM pointers
template <typename T> class com_ptr {
  public:
    com_ptr() = default;
    ~com_ptr() { reset(); }
    com_ptr(const com_ptr &) = delete;
    com_ptr &operator=(const com_ptr &) = delete;
    com_ptr(com_ptr &&o) noexcept : p_(o.p_) { o.p_ = nullptr; }
    com_ptr &operator=(com_ptr &&o) noexcept {
        if (this != &o) {
            reset();
            p_ = o.p_;
            o.p_ = nullptr;
        }
        return *this;
    }

    T *get() const { return p_; }
    T **put() {
        reset();
        return &p_;
    }
    T *operator->() const { return p_; }
    explicit operator bool() const { return p_ != nullptr; }

    void reset() {
        if (p_) {
            p_->Release();
            p_ = nullptr;
        }
    }

  private:
    T *p_ = nullptr;
};

using storage_ptr = com_ptr<IStorage>;
using stream_ptr = com_ptr<IStream>;
using enum_stat_ptr = com_ptr<IEnumSTATSTG>;
using propset_storage_ptr = com_ptr<IPropertySetStorage>;
using propset_ptr = com_ptr<IPropertyStorage>;

// Convert UTF-8 string_view to wide string for Win32 APIs
inline std::wstring to_wide(std::string_view s) {
    if (s.empty()) {
        return {};
    }
    int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring result(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), result.data(), len);
    return result;
}

// Convert wide string to UTF-8
inline std::string to_utf8(const wchar_t *ws) {
    if (!ws || !*ws) {
        return {};
    }
    int len = WideCharToMultiByte(CP_UTF8, 0, ws, -1, nullptr, 0, nullptr, nullptr);
    std::string result(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws, -1, result.data(), len, nullptr, nullptr);
    return result;
}

// Create a Win32 compound file (v3 = 512-byte sectors)
inline HRESULT win32_create_v3(const std::wstring &path, IStorage **out) {
    return StgCreateDocfile(path.c_str(), STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, out);
}

// Create a Win32 compound file (v4 = 4096-byte sectors)
inline HRESULT win32_create_v4(const std::wstring &path, IStorage **out) {
    STGOPTIONS opts{};
    opts.usVersion = 1;
    opts.ulSectorSize = 4096;
    return StgCreateStorageEx(path.c_str(), STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, STGFMT_DOCFILE, 0,
                              &opts, nullptr, IID_IStorage, reinterpret_cast<void **>(out));
}

// Open a Win32 compound file for reading
inline HRESULT win32_open_read(const std::wstring &path, IStorage **out) {
    return StgOpenStorage(path.c_str(), nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0, out);
}

// Open a Win32 compound file for read-write
inline HRESULT win32_open_rw(const std::wstring &path, IStorage **out) {
    return StgOpenStorage(path.c_str(), nullptr, STGM_READWRITE | STGM_SHARE_EXCLUSIVE, nullptr, 0, out);
}

} // namespace conformance

#endif // _WIN32
