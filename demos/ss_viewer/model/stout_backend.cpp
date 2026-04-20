/**
 * @file stout_backend.cpp
 * @brief Stout library backend implementation.
 */
#include "ss_viewer/model/stout_backend.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <span>

namespace ssv {

/// Helper: navigate to a storage by full_path from the compound_file root.
static std::optional<stout::storage> navigate_to_storage(stout::compound_file& cf, const std::string& full_path) {
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
    auto root = cf.root_storage();
    stout::storage stg = root;
    for (size_t i = 1; i < parts.size(); ++i) {
        auto sub = stg.open_storage(parts[i]);
        if (!sub) return std::nullopt;
        stg = std::move(*sub);
    }
    return stg;
}

/// Helper: fill an entry_info from an entry_stat.
static entry_info stat_to_entry(const stout::entry_stat& st, const std::string& parent_path) {
    entry_info info;
    info.name = st.name;
    info.type = st.type;
    info.size = st.size;
    info.clsid = st.clsid;
    info.creation_time = st.creation_time;
    info.modified_time = st.modified_time;
    info.state_bits = st.state_bits;
    info.full_path = parent_path.empty() ? st.name : parent_path + "/" + st.name;
    return info;
}

entry_info build_stout_tree(stout::storage& stg, const std::string& parent_path) {
    auto info = stat_to_entry(stg.stat(), parent_path);
    info.children_loaded = true;

    for (auto& child : stg.children()) {
        if (child.type == stout::entry_type::storage) {
            auto sub = stg.open_storage(child.name);
            if (sub) info.children.push_back(build_stout_tree(*sub, info.full_path));
        } else {
            auto ci = stat_to_entry(child, info.full_path);
            ci.children_loaded = true;  // streams have no children
            info.children.push_back(std::move(ci));
        }
    }
    return info;
}

entry_info build_stout_tree_shallow(stout::storage& stg, const std::string& parent_path) {
    auto info = stat_to_entry(stg.stat(), parent_path);
    info.children_loaded = true;  // first level is loaded

    for (auto& child : stg.children()) {
        auto ci = stat_to_entry(child, info.full_path);
        if (child.type == stout::entry_type::stream)
            ci.children_loaded = true;  // streams have no children
        // storages left with children_loaded = false (lazy)
        info.children.push_back(std::move(ci));
    }
    return info;
}

void load_stout_children(stout::compound_file& cf, entry_info& ei) {
    if (ei.children_loaded) return;
    if (ei.type != stout::entry_type::storage && ei.type != stout::entry_type::root) {
        ei.children_loaded = true;
        return;
    }

    auto stg_opt = navigate_to_storage(cf, ei.full_path);
    if (!stg_opt) {
        ei.children_loaded = true;
        return;
    }

    ei.children.clear();
    for (auto& child : stg_opt->children()) {
        auto ci = stat_to_entry(child, ei.full_path);
        if (child.type == stout::entry_type::stream)
            ci.children_loaded = true;
        ei.children.push_back(std::move(ci));
    }
    ei.children_loaded = true;
}

std::vector<uint8_t> read_stout_stream(stout::compound_file& cf,
                                        const entry_info& ei,
                                        uint64_t max_bytes) {
    auto root = cf.root_storage();
    // Parse path: "Root Entry/Storage1/Stream1"
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
    // Skip "Root Entry"
    stout::storage stg = root;
    for (size_t i = 1; i + 1 < parts.size(); ++i) {
        auto sub = stg.open_storage(parts[i]);
        if (!sub) return {};
        stg = std::move(*sub);
    }
    if (parts.size() < 2) return {};
    auto strm = stg.open_stream(parts.back());
    if (!strm) return {};

    uint64_t sz = std::min(strm->size(), max_bytes);
    std::vector<uint8_t> buf(static_cast<size_t>(sz));
    if (sz > 0) strm->read(0, std::span<uint8_t>(buf));
    return buf;
}

paged_reader open_stout_reader(stout::compound_file& cf, const entry_info& ei) {
    if (ei.type != stout::entry_type::stream) return {};

    // Navigate to the stream
    auto root = cf.root_storage();
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

    stout::storage stg = root;
    for (size_t i = 1; i + 1 < parts.size(); ++i) {
        auto sub = stg.open_storage(parts[i]);
        if (!sub) return {};
        stg = std::move(*sub);
    }
    if (parts.size() < 2) return {};
    auto strm_result = stg.open_stream(parts.back());
    if (!strm_result) return {};

    // Capture the stream by shared_ptr so the lambda outlives this scope
    auto strm = std::make_shared<stout::stream>(std::move(*strm_result));
    uint64_t sz = strm->size();

    return paged_reader(sz, [strm](uint64_t offset, std::span<uint8_t> buf) -> size_t {
        auto result = strm->read(offset, buf);
        return result ? *result : 0;
    });
}

} // namespace ssv
