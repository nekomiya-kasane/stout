#include "stout/cfb/directory.h"
#include <algorithm>
#include <cstring>

namespace stout::cfb {

auto parse_dir_entry(std::span<const uint8_t, dir_entry_size> d, bool is_v3) noexcept -> dir_entry {
    using namespace util;
    dir_entry e;

    // Name: offset 0, 64 bytes (UTF-16LE with null terminator)
    uint16_t name_len = read_u16_le(d.data() + 64); // offset 64: name size in bytes
    if (name_len >= 2 && name_len <= dir_name_max_bytes) {
        uint16_t char_count = (name_len / 2) - 1; // exclude null
        e.name.reserve(char_count);
        for (uint16_t i = 0; i < char_count; ++i) {
            auto lo = d[i * 2];
            auto hi = d[i * 2 + 1];
            e.name.push_back(static_cast<char16_t>(lo | (hi << 8)));
        }
    }

    // Type: offset 66
    e.type = static_cast<entry_type>(d[66]);

    // Color: offset 67
    e.color = static_cast<node_color>(d[67]);

    // Left sibling: offset 68
    e.left_sibling = read_u32_le(d.data() + 68);

    // Right sibling: offset 72
    e.right_sibling = read_u32_le(d.data() + 72);

    // Child: offset 76
    e.child = read_u32_le(d.data() + 76);

    // CLSID: offset 80, 16 bytes
    e.clsid = guid_read_le(d.data() + 80);

    // State bits: offset 96
    e.state_bits = read_u32_le(d.data() + 96);

    // Creation time: offset 100, 8 bytes
    e.creation_time = read_u64_le(d.data() + 100);

    // Modified time: offset 108, 8 bytes
    e.modified_time = read_u64_le(d.data() + 108);

    // Start sector: offset 116
    e.start_sector = read_u32_le(d.data() + 116);

    // Stream size: offset 120, 8 bytes (v3 uses only lower 32 bits)
    if (is_v3) {
        e.stream_size = read_u32_le(d.data() + 120);
    } else {
        e.stream_size = read_u64_le(d.data() + 120);
    }

    return e;
}

void serialize_dir_entry(const dir_entry& e, std::span<uint8_t, dir_entry_size> out, bool is_v3) noexcept {
    using namespace util;
    std::fill(out.begin(), out.end(), uint8_t{0});

    // Name: write UTF-16LE chars
    uint16_t char_count = static_cast<uint16_t>(std::min(e.name.size(), size_t{31}));
    for (uint16_t i = 0; i < char_count; ++i) {
        out[i * 2]     = static_cast<uint8_t>(e.name[i] & 0xFF);
        out[i * 2 + 1] = static_cast<uint8_t>(e.name[i] >> 8);
    }
    // Null terminator
    out[char_count * 2]     = 0;
    out[char_count * 2 + 1] = 0;

    // Name size in bytes (including null terminator)
    uint16_t name_bytes = (char_count + 1) * 2;
    if (e.is_empty()) name_bytes = 0;
    write_u16_le(out.data() + 64, name_bytes);

    // Type
    out[66] = static_cast<uint8_t>(e.type);

    // Color
    out[67] = static_cast<uint8_t>(e.color);

    // Left sibling
    write_u32_le(out.data() + 68, e.left_sibling);

    // Right sibling
    write_u32_le(out.data() + 72, e.right_sibling);

    // Child
    write_u32_le(out.data() + 76, e.child);

    // CLSID
    guid_write_le(out.data() + 80, e.clsid);

    // State bits
    write_u32_le(out.data() + 96, e.state_bits);

    // Creation time
    write_u64_le(out.data() + 100, e.creation_time);

    // Modified time
    write_u64_le(out.data() + 108, e.modified_time);

    // Start sector
    write_u32_le(out.data() + 116, e.start_sector);

    // Stream size
    if (is_v3) {
        write_u32_le(out.data() + 120, static_cast<uint32_t>(e.stream_size));
        write_u32_le(out.data() + 124, 0);
    } else {
        write_u64_le(out.data() + 120, e.stream_size);
    }
}

auto directory::find_child(uint32_t parent_id, std::u16string_view name) const -> uint32_t {
    if (parent_id >= entries_.size()) return nostream;
    uint32_t node = entries_[parent_id].child;

    while (node != nostream && node < entries_.size()) {
        int cmp = util::cfb_name_compare(name, entries_[node].name);
        if (cmp == 0) return node;
        if (cmp < 0) node = entries_[node].left_sibling;
        else          node = entries_[node].right_sibling;
    }
    return nostream;
}

void directory::insert_child(uint32_t parent_id, uint32_t new_entry_id) {
    if (parent_id >= entries_.size() || new_entry_id >= entries_.size()) return;

    auto& parent = entries_[parent_id];
    auto& new_entry = entries_[new_entry_id];
    new_entry.left_sibling = nostream;
    new_entry.right_sibling = nostream;
    new_entry.color = node_color::red;

    if (parent.child == nostream) {
        // First child — make it the root of the tree, colored black
        parent.child = new_entry_id;
        new_entry.color = node_color::black;
        return;
    }

    // BST insert
    uint32_t cur = parent.child;
    uint32_t par = nostream;
    while (cur != nostream && cur < entries_.size()) {
        par = cur;
        int cmp = util::cfb_name_compare(new_entry.name, entries_[cur].name);
        if (cmp < 0) cur = entries_[cur].left_sibling;
        else          cur = entries_[cur].right_sibling;
    }

    if (par != nostream) {
        int cmp = util::cfb_name_compare(new_entry.name, entries_[par].name);
        if (cmp < 0) entries_[par].left_sibling = new_entry_id;
        else          entries_[par].right_sibling = new_entry_id;
    }

    // Red-black fixup
    rb_insert_fixup(parent_id, new_entry_id);
}

void directory::remove_child(uint32_t parent_id, uint32_t entry_id) {
    // Simplified removal: rebuild the tree without the entry
    if (parent_id >= entries_.size()) return;

    std::vector<uint32_t> children;
    enumerate_children(parent_id, [&](uint32_t id, const dir_entry&) {
        if (id != entry_id) children.push_back(id);
    });

    // Clear the entry
    entries_[entry_id] = dir_entry{};

    // Rebuild tree
    entries_[parent_id].child = nostream;
    for (auto id : children) {
        entries_[id].left_sibling = nostream;
        entries_[id].right_sibling = nostream;
        entries_[id].color = node_color::red;
        insert_child(parent_id, id);
    }
}

void directory::enumerate_children(uint32_t parent_id,
                                    const std::function<void(uint32_t, const dir_entry&)>& callback) const {
    if (parent_id >= entries_.size()) return;
    inorder_traverse(entries_[parent_id].child, callback);
}

void directory::inorder_traverse(uint32_t node_id,
                                  const std::function<void(uint32_t, const dir_entry&)>& callback) const {
    if (node_id == nostream || node_id >= entries_.size()) return;
    inorder_traverse(entries_[node_id].left_sibling, callback);
    callback(node_id, entries_[node_id]);
    inorder_traverse(entries_[node_id].right_sibling, callback);
}

auto directory::find_parent_in_tree(uint32_t root_id, uint32_t node_id) const -> uint32_t {
    if (root_id == nostream || root_id >= entries_.size()) return nostream;
    if (root_id == node_id) return nostream;

    // BFS/DFS to find parent
    std::vector<uint32_t> stack = {root_id};
    while (!stack.empty()) {
        auto cur = stack.back();
        stack.pop_back();
        if (cur == nostream || cur >= entries_.size()) continue;
        if (entries_[cur].left_sibling == node_id || entries_[cur].right_sibling == node_id)
            return cur;
        if (entries_[cur].left_sibling != nostream)
            stack.push_back(entries_[cur].left_sibling);
        if (entries_[cur].right_sibling != nostream)
            stack.push_back(entries_[cur].right_sibling);
    }
    return nostream;
}

void directory::rb_rotate_left(uint32_t parent_id, uint32_t x) {
    auto& parent = entries_[parent_id];
    uint32_t y = entries_[x].right_sibling;
    if (y == nostream || y >= entries_.size()) return;

    entries_[x].right_sibling = entries_[y].left_sibling;
    uint32_t px = find_parent_in_tree(parent.child, x);

    if (px == nostream) {
        parent.child = y;
    } else if (entries_[px].left_sibling == x) {
        entries_[px].left_sibling = y;
    } else {
        entries_[px].right_sibling = y;
    }
    entries_[y].left_sibling = x;
}

void directory::rb_rotate_right(uint32_t parent_id, uint32_t x) {
    auto& parent = entries_[parent_id];
    uint32_t y = entries_[x].left_sibling;
    if (y == nostream || y >= entries_.size()) return;

    entries_[x].left_sibling = entries_[y].right_sibling;
    uint32_t px = find_parent_in_tree(parent.child, x);

    if (px == nostream) {
        parent.child = y;
    } else if (entries_[px].left_sibling == x) {
        entries_[px].left_sibling = y;
    } else {
        entries_[px].right_sibling = y;
    }
    entries_[y].right_sibling = x;
}

void directory::rb_insert_fixup(uint32_t parent_id, uint32_t z) {
    auto& parent = entries_[parent_id];

    auto color_of = [&](uint32_t id) -> node_color {
        if (id == nostream || id >= entries_.size()) return node_color::black;
        return entries_[id].color;
    };

    while (true) {
        uint32_t zp = find_parent_in_tree(parent.child, z);
        if (zp == nostream || color_of(zp) == node_color::black) break;

        uint32_t zpp = find_parent_in_tree(parent.child, zp);
        if (zpp == nostream) break;

        if (zp == entries_[zpp].left_sibling) {
            uint32_t uncle = entries_[zpp].right_sibling;
            if (color_of(uncle) == node_color::red) {
                entries_[zp].color = node_color::black;
                if (uncle != nostream && uncle < entries_.size())
                    entries_[uncle].color = node_color::black;
                entries_[zpp].color = node_color::red;
                z = zpp;
            } else {
                if (z == entries_[zp].right_sibling) {
                    z = zp;
                    rb_rotate_left(parent_id, z);
                    zp = find_parent_in_tree(parent.child, z);
                    if (zp == nostream) break;
                    zpp = find_parent_in_tree(parent.child, zp);
                    if (zpp == nostream) break;
                }
                entries_[zp].color = node_color::black;
                entries_[zpp].color = node_color::red;
                rb_rotate_right(parent_id, zpp);
            }
        } else {
            uint32_t uncle = entries_[zpp].left_sibling;
            if (color_of(uncle) == node_color::red) {
                entries_[zp].color = node_color::black;
                if (uncle != nostream && uncle < entries_.size())
                    entries_[uncle].color = node_color::black;
                entries_[zpp].color = node_color::red;
                z = zpp;
            } else {
                if (z == entries_[zp].left_sibling) {
                    z = zp;
                    rb_rotate_right(parent_id, z);
                    zp = find_parent_in_tree(parent.child, z);
                    if (zp == nostream) break;
                    zpp = find_parent_in_tree(parent.child, zp);
                    if (zpp == nostream) break;
                }
                entries_[zp].color = node_color::black;
                entries_[zpp].color = node_color::red;
                rb_rotate_left(parent_id, zpp);
            }
        }
    }

    // Root of tree is always black
    if (parent.child != nostream && parent.child < entries_.size()) {
        entries_[parent.child].color = node_color::black;
    }
}

} // namespace stout::cfb
