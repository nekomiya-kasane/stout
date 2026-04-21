#include "stout/compound_file.h"

#include "stout/util/filetime.h"
#include "stout/version.h"

#include <algorithm>
#include <cstring>
#include <variant>

namespace stout {

auto library_version() noexcept -> version_info {
    return {0, 1, 0};
}

auto library_version_string() noexcept -> const char * {
    return "0.1.0";
}

// ── compound_file::internal ────────────────────────────────────────────

struct compound_file::internal {
    std::variant<io::memory_lock_bytes, io::file_lock_bytes> backend;
    cfb::cfb_header header;
    cfb::fat_table fat;
    cfb::difat_table difat;
    cfb::mini_fat_table mini_fat;
    cfb::directory dir;
    cfb::mini_stream_io mini_sio;
    bool is_memory = false;
    bool is_v3 = false;
    open_mode mode = open_mode::read;

    // Transaction snapshot
    struct snapshot {
        cfb::cfb_header header;
        cfb::fat_table fat;
        cfb::difat_table difat;
        cfb::mini_fat_table mini_fat;
        cfb::directory dir;
        std::vector<uint8_t> backing_data; // for memory backends only
    };
    std::unique_ptr<snapshot> txn_snapshot;
    bool in_txn = false;

    auto sector_size() const noexcept -> uint32_t { return header.sector_size(); }

    // Dispatch sector_io operations through the variant
    template <typename Fn> auto with_sio(Fn &&fn) {
        if (is_memory) {
            auto &mlb = std::get<io::memory_lock_bytes>(backend);
            cfb::sector_io sio(mlb, sector_size());
            return fn(sio);
        } else {
            auto &flb = std::get<io::file_lock_bytes>(backend);
            cfb::sector_io sio(flb, sector_size());
            return fn(sio);
        }
    }

    void rebuild_mini_stream_io() {
        auto root_chain = fat.chain(dir.root().start_sector);
        mini_sio.init(std::move(root_chain), sector_size(), header.mini_sector_size_val());
    }

    // Walk a FAT chain to find its length and tail sector (avoids heap alloc)
    auto chain_length_and_tail(uint32_t start) const -> std::pair<uint32_t, uint32_t> {
        uint32_t len = 0, tail = cfb::endofchain;
        for (uint32_t cur = start; cur != cfb::endofchain && cur != cfb::freesect && cur < fat.size();
             cur = fat.next(cur)) {
            tail = cur;
            ++len;
        }
        return {len, tail};
    }

    // Ensure the root entry's mini stream container has enough regular sectors
    // to hold all allocated mini sectors.
    void ensure_mini_stream_container(uint64_t /*hint_ignored*/) {
        auto ss = sector_size();
        auto mss = header.mini_sector_size_val();

        // Count total mini sectors used across all streams
        uint64_t total_mini_sectors = mini_fat.size();
        uint64_t needed_bytes = total_mini_sectors * mss;
        uint64_t needed_sectors = (needed_bytes + ss - 1) / ss;

        // Ensure we have a mini FAT sector allocated
        if (header.first_mini_fat_sector == cfb::endofchain) {
            auto mfat_sec = fat.allocate();
            fat.set(mfat_sec, cfb::endofchain);
            header.first_mini_fat_sector = mfat_sec;
            header.total_mini_fat_sectors = 1;
        }

        auto &root = dir.root();
        auto [current_sectors, tail] = chain_length_and_tail(root.start_sector);

        // Grow the root entry's sector chain if needed
        while (current_sectors < needed_sectors) {
            auto new_id = fat.allocate();
            if (root.start_sector == cfb::endofchain) {
                root.start_sector = new_id;
                tail = new_id;
            } else {
                fat.set(tail, new_id);
                tail = new_id;
            }
            ++current_sectors;
        }

        // Update root stream size
        root.stream_size = needed_bytes;

        ensure_file_size();
        rebuild_mini_stream_io();
    }

    // Ensure the directory sector chain has enough sectors for all entries
    void ensure_dir_chain() {
        auto ss = sector_size();
        auto entries_per = cfb::dir_entries_per_sector(ss);
        uint64_t needed_sectors = (dir.count() + entries_per - 1) / entries_per;

        auto [current_sectors, tail] = chain_length_and_tail(header.first_dir_sector);

        while (current_sectors < needed_sectors) {
            auto new_id = fat.allocate();
            if (header.first_dir_sector == cfb::endofchain) {
                header.first_dir_sector = new_id;
                tail = new_id;
            } else {
                fat.set(tail, new_id);
                tail = new_id;
            }
            ++current_sectors;
        }
    }

    // Ensure the backing store is large enough for all allocated sectors
    void ensure_file_size() {
        auto ss = sector_size();
        auto max_sector = fat.size();
        uint64_t min_file_size = ss + max_sector * static_cast<uint64_t>(ss);
        if (is_memory) {
            auto &mlb = std::get<io::memory_lock_bytes>(backend);
            auto cur = mlb.size();
            if (cur && *cur < min_file_size) {
                mlb.set_size(min_file_size);
            }
        } else {
            auto &flb = std::get<io::file_lock_bytes>(backend);
            auto cur_size = flb.size();
            if (cur_size && *cur_size < min_file_size) {
                flb.set_size(min_file_size);
            }
        }
    }
};

struct compound_file::impl {
    internal state;
};

// ── compound_file lifecycle ────────────────────────────────────────────

compound_file::compound_file() : impl_(std::make_unique<impl>()) {}
compound_file::~compound_file() = default;
compound_file::compound_file(compound_file &&) noexcept = default;
compound_file &compound_file::operator=(compound_file &&) noexcept = default;

auto compound_file::internals() -> internal & {
    return impl_->state;
}

auto compound_file::version() const noexcept -> cfb_version {
    return impl_->state.header.is_v3() ? cfb_version::v3 : cfb_version::v4;
}

auto compound_file::data() const -> const std::vector<uint8_t> * {
    if (impl_->state.is_memory) {
        return &std::get<io::memory_lock_bytes>(impl_->state.backend).data();
    }
    return nullptr;
}

// ── transaction support ────────────────────────────────────────────────

auto compound_file::in_transaction() const noexcept -> bool {
    return impl_->state.in_txn;
}

auto compound_file::begin_transaction() -> std::expected<void, error> {
    auto &s = impl_->state;
    if (s.in_txn) {
        return std::unexpected(error::transaction_failed);
    }

    auto snap = std::make_unique<internal::snapshot>();
    snap->header = s.header;
    snap->fat = s.fat;
    snap->difat = s.difat;
    snap->mini_fat = s.mini_fat;
    snap->dir = s.dir;

    // For memory backends, snapshot the raw bytes too
    if (s.is_memory) {
        snap->backing_data = std::get<io::memory_lock_bytes>(s.backend).data();
    }

    s.txn_snapshot = std::move(snap);
    s.in_txn = true;
    return {};
}

auto compound_file::commit() -> std::expected<void, error> {
    auto &s = impl_->state;
    if (!s.in_txn) {
        return std::unexpected(error::transaction_failed);
    }

    // Flush all changes to the backing store
    auto r = flush();
    if (!r) {
        return std::unexpected(r.error());
    }

    // Discard the snapshot
    s.txn_snapshot.reset();
    s.in_txn = false;
    return {};
}

auto compound_file::revert() -> std::expected<void, error> {
    auto &s = impl_->state;
    if (!s.in_txn || !s.txn_snapshot) {
        return std::unexpected(error::transaction_failed);
    }

    // Restore metadata from snapshot
    s.header = s.txn_snapshot->header;
    s.fat = s.txn_snapshot->fat;
    s.difat = s.txn_snapshot->difat;
    s.mini_fat = s.txn_snapshot->mini_fat;
    s.dir = s.txn_snapshot->dir;

    // For memory backends, restore the raw bytes
    if (s.is_memory) {
        std::get<io::memory_lock_bytes>(s.backend).data() = s.txn_snapshot->backing_data;
    } else {
        // For file backends, re-read from disk to restore sector data
        // The metadata is already restored; flush it back to overwrite changes
        auto r = flush();
        if (!r) {
            return std::unexpected(r.error());
        }
    }

    s.rebuild_mini_stream_io();
    s.txn_snapshot.reset();
    s.in_txn = false;
    return {};
}

// ── create_in_memory ───────────────────────────────────────────────────

auto compound_file::create_in_memory(cfb_version version) -> std::expected<compound_file, error> {
    compound_file cf;
    auto &s = cf.impl_->state;
    s.is_memory = true;
    s.mode = open_mode::read_write;
    s.is_v3 = (version == cfb_version::v3);

    // Build default header
    s.header = cfb::make_default_header(version);
    auto ss = s.header.sector_size();

    // We need at minimum: header + 1 FAT sector + 1 directory sector
    auto &mlb = s.backend.emplace<io::memory_lock_bytes>();

    // Allocate FAT: sector 0 = FAT, sector 1 = directory
    s.fat.resize(cfb::fat_entries_per_sector(ss));
    s.fat.set(0, cfb::fatsect);    // sector 0 is a FAT sector
    s.fat.set(1, cfb::endofchain); // sector 1 is directory (single sector)

    s.header.total_fat_sectors = 1;
    s.header.first_dir_sector = 1;
    // v4 requires total_dir_sectors to be the actual count
    if (!s.is_v3) {
        s.header.total_dir_sectors = 1;
    }
    s.header.difat[0] = 0; // FAT sector 0

    // Set file size: header + 2 sectors
    uint64_t file_size = ss + ss * 2;
    if (s.is_v3) {
        file_size = 512 + ss * 2; // v3 header is 512 bytes, sectors are 512
    }
    mlb.set_size(file_size);

    // Write header
    cfb::sector_io sio(mlb, ss);
    std::array<uint8_t, cfb::header_size> hdr_buf = {};
    cfb::serialize_header(s.header, hdr_buf);
    // For v4, the header occupies the first 4096 bytes (padded)
    std::vector<uint8_t> hdr_full(ss, 0);
    std::copy_n(hdr_buf.begin(), cfb::header_size, hdr_full.begin());
    mlb.write_at(0, hdr_full);

    // Write FAT sector
    std::array<uint32_t, 1> fat_sectors = {0};
    s.fat.flush(sio, std::span<const uint32_t>{fat_sectors});

    // Create root directory entry
    auto root_id = s.dir.add_entry();
    s.dir.entry(root_id).name = u"Root Entry";
    s.dir.entry(root_id).type = entry_type::root;
    s.dir.entry(root_id).color = cfb::node_color::black;
    s.dir.entry(root_id).start_sector = cfb::endofchain;

    // Write directory
    s.dir.flush(sio, s.fat, s.header.first_dir_sector);

    // Load DIFAT
    s.difat.add_fat_sector(0);

    return cf;
}

// ── open_from_memory ───────────────────────────────────────────────────

auto compound_file::open_from_memory(std::vector<uint8_t> data) -> std::expected<compound_file, error> {
    if (data.size() < cfb::header_size) {
        return std::unexpected(error::invalid_header);
    }

    compound_file cf;
    auto &s = cf.impl_->state;
    s.is_memory = true;
    s.mode = open_mode::read_write;

    auto &mlb = s.backend.emplace<io::memory_lock_bytes>(std::move(data));

    // Parse header
    std::array<uint8_t, cfb::header_size> hdr_buf;
    mlb.read_at(0, hdr_buf);
    auto hdr_result = cfb::parse_header(hdr_buf);
    if (!hdr_result) {
        return std::unexpected(hdr_result.error());
    }
    s.header = *hdr_result;

    auto val = cfb::validate_header(s.header);
    if (!val) {
        return std::unexpected(val.error());
    }

    s.is_v3 = s.header.is_v3();
    auto ss = s.header.sector_size();
    cfb::sector_io sio(mlb, ss);

    // Load DIFAT
    auto difat_r = s.difat.load(s.header, sio);
    if (!difat_r) {
        return std::unexpected(difat_r.error());
    }

    // Load FAT
    auto fat_r = s.fat.load(sio, std::span<const uint32_t>{s.difat.fat_sector_ids()});
    if (!fat_r) {
        return std::unexpected(fat_r.error());
    }

    // Load directory
    auto dir_r = s.dir.load(sio, s.fat, s.header.first_dir_sector, s.is_v3);
    if (!dir_r) {
        return std::unexpected(dir_r.error());
    }

    // Load mini FAT
    auto mfat_r = s.mini_fat.load(sio, s.fat, s.header.first_mini_fat_sector);
    if (!mfat_r) {
        return std::unexpected(mfat_r.error());
    }

    // Set up mini stream I/O
    s.rebuild_mini_stream_io();

    return cf;
}

// ── open (file) ────────────────────────────────────────────────────────

auto compound_file::open(const std::filesystem::path &path, open_mode mode) -> std::expected<compound_file, error> {
    auto flb_result = io::file_lock_bytes::open(path, mode);
    if (!flb_result) {
        return std::unexpected(flb_result.error());
    }

    compound_file cf;
    auto &s = cf.impl_->state;
    s.is_memory = false;
    s.mode = mode;
    s.backend.emplace<io::file_lock_bytes>(std::move(*flb_result));

    auto &flb = std::get<io::file_lock_bytes>(s.backend);

    // Parse header
    std::array<uint8_t, cfb::header_size> hdr_buf;
    auto rd = flb.read_at(0, hdr_buf);
    if (!rd || *rd < cfb::header_size) {
        return std::unexpected(error::invalid_header);
    }

    auto hdr_result = cfb::parse_header(hdr_buf);
    if (!hdr_result) {
        return std::unexpected(hdr_result.error());
    }
    s.header = *hdr_result;

    auto val = cfb::validate_header(s.header);
    if (!val) {
        return std::unexpected(val.error());
    }

    s.is_v3 = s.header.is_v3();
    auto ss = s.header.sector_size();
    cfb::sector_io sio(flb, ss);

    // Load DIFAT
    auto difat_r = s.difat.load(s.header, sio);
    if (!difat_r) {
        return std::unexpected(difat_r.error());
    }

    // Load FAT
    auto fat_r = s.fat.load(sio, std::span<const uint32_t>{s.difat.fat_sector_ids()});
    if (!fat_r) {
        return std::unexpected(fat_r.error());
    }

    // Load directory
    auto dir_r = s.dir.load(sio, s.fat, s.header.first_dir_sector, s.is_v3);
    if (!dir_r) {
        return std::unexpected(dir_r.error());
    }

    // Load mini FAT
    auto mfat_r = s.mini_fat.load(sio, s.fat, s.header.first_mini_fat_sector);
    if (!mfat_r) {
        return std::unexpected(mfat_r.error());
    }

    s.rebuild_mini_stream_io();

    return cf;
}

// ── create (file) ──────────────────────────────────────────────────────

auto compound_file::create(const std::filesystem::path &path, cfb_version version)
    -> std::expected<compound_file, error> {
    // Open file handle directly
    auto flb_result = io::file_lock_bytes::open(path, open_mode::read_write);
    if (!flb_result) {
        return std::unexpected(flb_result.error());
    }

    compound_file cf;
    auto &s = cf.impl_->state;
    s.is_memory = false;
    s.mode = open_mode::read_write;
    s.is_v3 = (version == cfb_version::v3);

    // Build default header
    s.header = cfb::make_default_header(version);
    auto ss = s.header.sector_size();

    auto &flb = s.backend.emplace<io::file_lock_bytes>(std::move(*flb_result));

    // Allocate FAT: sector 0 = FAT, sector 1 = directory
    s.fat.resize(cfb::fat_entries_per_sector(ss));
    s.fat.set(0, cfb::fatsect);
    s.fat.set(1, cfb::endofchain);

    s.header.total_fat_sectors = 1;
    s.header.first_dir_sector = 1;
    if (!s.is_v3) {
        s.header.total_dir_sectors = 1;
    }
    s.header.difat[0] = 0;

    // Write header + FAT + directory directly through the page cache
    cfb::sector_io sio(flb, ss);

    std::array<uint8_t, cfb::header_size> hdr_buf = {};
    cfb::serialize_header(s.header, hdr_buf);
    std::vector<uint8_t> hdr_full(ss, 0);
    std::copy_n(hdr_buf.begin(), cfb::header_size, hdr_full.begin());
    flb.write_at(0, hdr_full);

    // Write FAT sector
    std::array<uint32_t, 1> fat_sectors = {0};
    s.fat.flush(sio, std::span<const uint32_t>{fat_sectors});

    // Create root directory entry
    auto root_id = s.dir.add_entry();
    s.dir.entry(root_id).name = u"Root Entry";
    s.dir.entry(root_id).type = entry_type::root;
    s.dir.entry(root_id).color = cfb::node_color::black;
    s.dir.entry(root_id).start_sector = cfb::endofchain;

    // Write directory
    s.dir.flush(sio, s.fat, s.header.first_dir_sector);

    // Load DIFAT
    s.difat.add_fat_sector(0);

    return cf;
}

// ── flush ──────────────────────────────────────────────────────────────

auto compound_file::flush() -> std::expected<void, error> {
    auto &s = impl_->state;

    // Ensure directory chain and file size are adequate before flushing
    s.ensure_dir_chain();
    s.ensure_file_size();

    // v4 requires total_dir_sectors to reflect the actual directory chain length
    if (!s.is_v3) {
        auto [dir_count, _] = s.chain_length_and_tail(s.header.first_dir_sector);
        s.header.total_dir_sectors = dir_count;
    }

    return s.with_sio([&](auto &sio) -> std::expected<void, error> {
        // Flush FAT
        auto fat_r = s.fat.flush(sio, std::span<const uint32_t>{s.difat.fat_sector_ids()});
        if (!fat_r) {
            return std::unexpected(fat_r.error());
        }

        // Flush mini FAT
        if (s.header.first_mini_fat_sector != cfb::endofchain) {
            auto mfat_r = s.mini_fat.flush(sio, s.fat, s.header.first_mini_fat_sector);
            if (!mfat_r) {
                return std::unexpected(mfat_r.error());
            }
        }

        // Flush directory
        auto dir_r = s.dir.flush(sio, s.fat, s.header.first_dir_sector);
        if (!dir_r) {
            return std::unexpected(dir_r.error());
        }

        // Flush header
        std::array<uint8_t, cfb::header_size> hdr_buf = {};
        cfb::serialize_header(s.header, hdr_buf);
        std::vector<uint8_t> hdr_full(std::max(s.sector_size(), uint32_t{512}), 0);
        std::copy_n(hdr_buf.begin(), cfb::header_size, hdr_full.begin());
        if (s.is_memory) {
            std::get<io::memory_lock_bytes>(s.backend).write_at(0, hdr_full);
        } else {
            std::get<io::file_lock_bytes>(s.backend).write_at(0, hdr_full);
        }

        return sio.flush();
    });
}

// ── root_storage ───────────────────────────────────────────────────────

auto compound_file::root_storage() -> storage {
    return storage(impl_->state, 0);
}

// ── storage implementation ─────────────────────────────────────────────

storage::storage(compound_file::internal &cf, uint32_t dir_id) : cf_(&cf), dir_id_(dir_id) {}

auto storage::name() const -> std::string {
    return cf_->dir.entry(dir_id_).utf8_name();
}

auto storage::stat() const -> entry_stat {
    return cf_->dir.entry(dir_id_).to_stat();
}

auto storage::exists(std::string_view name) const -> bool {
    auto u16 = util::utf8_to_utf16le(name);
    return cf_->dir.find_child(dir_id_, u16) != cfb::nostream;
}

auto storage::children() const -> std::vector<entry_stat> {
    std::vector<entry_stat> result;
    cf_->dir.enumerate_children(dir_id_, [&](uint32_t, const cfb::dir_entry &e) { result.push_back(e.to_stat()); });
    return result;
}

auto storage::create_stream(std::string_view name) -> std::expected<stream, error> {
    auto u16 = util::utf8_to_utf16le(name);

    // Check if already exists
    auto existing = cf_->dir.find_child(dir_id_, u16);
    if (existing != cfb::nostream) {
        if (cf_->dir.entry(existing).is_stream()) {
            return stream(*cf_, existing);
        }
        return std::unexpected(error::already_exists);
    }

    auto id = cf_->dir.add_entry();
    cf_->dir.entry(id).name = u16;
    cf_->dir.entry(id).type = entry_type::stream;
    cf_->dir.entry(id).start_sector = cfb::endofchain;
    cf_->dir.entry(id).stream_size = 0;
    cf_->dir.insert_child(dir_id_, id);

    return stream(*cf_, id);
}

auto storage::open_stream(std::string_view name) -> std::expected<stream, error> {
    auto u16 = util::utf8_to_utf16le(name);
    auto id = cf_->dir.find_child(dir_id_, u16);
    if (id == cfb::nostream) {
        return std::unexpected(error::not_found);
    }
    if (!cf_->dir.entry(id).is_stream()) {
        return std::unexpected(error::not_found);
    }
    return stream(*cf_, id);
}

auto storage::create_storage(std::string_view name) -> std::expected<storage, error> {
    auto u16 = util::utf8_to_utf16le(name);

    auto existing = cf_->dir.find_child(dir_id_, u16);
    if (existing != cfb::nostream) {
        if (cf_->dir.entry(existing).is_storage()) {
            return storage(*cf_, existing);
        }
        return std::unexpected(error::already_exists);
    }

    auto id = cf_->dir.add_entry();
    cf_->dir.entry(id).name = u16;
    cf_->dir.entry(id).type = entry_type::storage;
    cf_->dir.insert_child(dir_id_, id);

    return storage(*cf_, id);
}

auto storage::open_storage(std::string_view name) -> std::expected<storage, error> {
    auto u16 = util::utf8_to_utf16le(name);
    auto id = cf_->dir.find_child(dir_id_, u16);
    if (id == cfb::nostream) {
        return std::unexpected(error::not_found);
    }
    if (!cf_->dir.entry(id).is_storage()) {
        return std::unexpected(error::not_found);
    }
    return storage(*cf_, id);
}

auto storage::remove(std::string_view name) -> std::expected<void, error> {
    auto u16 = util::utf8_to_utf16le(name);
    auto id = cf_->dir.find_child(dir_id_, u16);
    if (id == cfb::nostream) {
        return std::unexpected(error::not_found);
    }

    // Free the stream's sector chain
    auto &entry = cf_->dir.entry(id);
    if (entry.is_stream() && entry.start_sector != cfb::endofchain) {
        if (cfb::use_mini_stream(entry.stream_size)) {
            cf_->mini_fat.free_chain(entry.start_sector);
        } else {
            cf_->fat.free_chain(entry.start_sector);
        }
    }

    cf_->dir.remove_child(dir_id_, id);
    return {};
}

auto storage::rename(std::string_view new_name) -> std::expected<void, error> {
    if (new_name.empty()) {
        return std::unexpected(error::invalid_name);
    }
    if (new_name.size() > 31) {
        return std::unexpected(error::invalid_name);
    }
    auto u16 = util::utf8_to_utf16le(new_name);
    cf_->dir.entry(dir_id_).name = u16;
    return {};
}

auto storage::clsid() const -> guid {
    return cf_->dir.entry(dir_id_).clsid;
}

auto storage::set_clsid(const guid &id) -> void {
    cf_->dir.entry(dir_id_).clsid = id;
}

auto storage::state_bits() const -> uint32_t {
    return cf_->dir.entry(dir_id_).state_bits;
}

auto storage::set_state_bits(uint32_t bits) -> void {
    cf_->dir.entry(dir_id_).state_bits = bits;
}

auto storage::set_state_bits(uint32_t bits, uint32_t mask) -> void {
    auto &entry = cf_->dir.entry(dir_id_);
    entry.state_bits = (entry.state_bits & ~mask) | (bits & mask);
}

auto storage::creation_time() const -> file_time {
    return util::filetime_to_timepoint(cf_->dir.entry(dir_id_).creation_time);
}

auto storage::modified_time() const -> file_time {
    return util::filetime_to_timepoint(cf_->dir.entry(dir_id_).modified_time);
}

auto storage::set_creation_time(file_time t) -> void {
    cf_->dir.entry(dir_id_).creation_time = util::timepoint_to_filetime(t);
}

auto storage::set_modified_time(file_time t) -> void {
    cf_->dir.entry(dir_id_).modified_time = util::timepoint_to_filetime(t);
}

auto storage::set_element_times(std::string_view name, file_time ctime, file_time mtime) -> std::expected<void, error> {
    auto u16 = util::utf8_to_utf16le(name);
    auto id = cf_->dir.find_child(dir_id_, u16);
    if (id == cfb::nostream) {
        return std::unexpected(error::not_found);
    }
    auto &entry = cf_->dir.entry(id);
    entry.creation_time = util::timepoint_to_filetime(ctime);
    entry.modified_time = util::timepoint_to_filetime(mtime);
    return {};
}

auto storage::copy_to(storage &dest, std::string_view name) -> std::expected<void, error> {
    auto u16 = util::utf8_to_utf16le(name);
    auto id = cf_->dir.find_child(dir_id_, u16);
    if (id == cfb::nostream) {
        return std::unexpected(error::not_found);
    }
    auto &src_entry = cf_->dir.entry(id);

    if (src_entry.is_stream()) {
        // Copy stream data
        auto dst_strm = dest.create_stream(name);
        if (!dst_strm) {
            return std::unexpected(dst_strm.error());
        }
        auto sz = src_entry.stream_size;
        if (sz > 0) {
            std::vector<uint8_t> buf(static_cast<size_t>(sz));
            stream src_s(*cf_, id);
            auto rd = src_s.read(0, std::span<uint8_t>(buf));
            if (!rd) {
                return std::unexpected(rd.error());
            }
            auto wr = dst_strm->write(0, std::span<const uint8_t>(buf));
            if (!wr) {
                return std::unexpected(wr.error());
            }
        }
    } else if (src_entry.is_storage()) {
        // Copy storage recursively
        auto dst_sub = dest.create_storage(name);
        if (!dst_sub) {
            return std::unexpected(dst_sub.error());
        }
        storage src_stg(*cf_, id);
        auto kids = src_stg.children();
        for (auto &child : kids) {
            auto r = src_stg.copy_to(*dst_sub, child.name);
            if (!r) {
                return std::unexpected(r.error());
            }
        }
        // Copy metadata
        dst_sub->set_clsid(src_entry.clsid);
        dst_sub->set_state_bits(src_entry.state_bits);
    }
    return {};
}

// ── stream implementation ──────────────────────────────────────────────

stream::stream(compound_file::internal &cf, uint32_t dir_id) : cf_(&cf), dir_id_(dir_id) {}

auto stream::name() const -> std::string {
    return cf_->dir.entry(dir_id_).utf8_name();
}

auto stream::stat() const -> entry_stat {
    return cf_->dir.entry(dir_id_).to_stat();
}

auto stream::size() const -> uint64_t {
    return cf_->dir.entry(dir_id_).stream_size;
}

auto stream::read(uint64_t offset, std::span<uint8_t> buf) -> std::expected<size_t, error> {
    auto &entry = cf_->dir.entry(dir_id_);
    if (offset >= entry.stream_size) {
        return size_t{0};
    }

    auto available = entry.stream_size - offset;
    auto to_read = std::min(static_cast<size_t>(available), buf.size());

    if (cfb::use_mini_stream(entry.stream_size)) {
        // Read from mini stream — walk mini FAT chain directly
        return cf_->with_sio([&](auto &sio) -> std::expected<size_t, error> {
            uint32_t mss = cf_->mini_sio.mini_sec_size();
            uint32_t skip = static_cast<uint32_t>(offset / mss);
            uint32_t off_in_sec = static_cast<uint32_t>(offset % mss);

            // Walk to the starting mini sector
            uint32_t cur = entry.start_sector;
            for (uint32_t i = 0; i < skip && cur != cfb::endofchain; ++i) {
                cur = cf_->mini_fat.next(cur);
            }

            size_t bytes_read = 0;
            uint8_t sec_buf[64]; // mini sector is always 64 bytes
            bool first = true;
            while (cur != cfb::endofchain && cur != cfb::freesect && bytes_read < to_read) {
                auto r = cf_->mini_sio.read_mini_sector(sio, cur, std::span<uint8_t>(sec_buf, mss));
                if (!r) {
                    return std::unexpected(r.error());
                }
                uint32_t start = first ? off_in_sec : 0;
                first = false;
                auto avail = mss - start;
                auto copy_len = std::min(static_cast<size_t>(avail), to_read - bytes_read);
                std::copy_n(sec_buf + start, copy_len, buf.data() + bytes_read);
                bytes_read += copy_len;
                cur = cf_->mini_fat.next(cur);
            }
            return bytes_read;
        });
    } else {
        // Read from regular sectors — walk FAT chain directly
        return cf_->with_sio([&](auto &sio) -> std::expected<size_t, error> {
            auto ss = cf_->sector_size();
            uint32_t skip = static_cast<uint32_t>(offset / ss);
            uint32_t off_in_sec = static_cast<uint32_t>(offset % ss);

            // Walk to the starting sector
            uint32_t cur = entry.start_sector;
            for (uint32_t i = 0; i < skip && cur != cfb::endofchain; ++i) {
                cur = cf_->fat.next(cur);
            }

            size_t bytes_read = 0;
            bool first = true;
            while (cur != cfb::endofchain && cur != cfb::freesect && bytes_read < to_read) {
                uint32_t start = first ? off_in_sec : 0;
                first = false;
                auto avail = ss - start;
                auto copy_len = std::min(static_cast<size_t>(avail), to_read - bytes_read);
                // Use direct partial read — no intermediate buffer needed
                auto r = sio.read_at(cur, start, buf.subspan(bytes_read, copy_len));
                if (!r) {
                    return std::unexpected(r.error());
                }
                bytes_read += copy_len;
                cur = cf_->fat.next(cur);
            }
            return bytes_read;
        });
    }
}

auto stream::write(uint64_t offset, std::span<const uint8_t> buf) -> std::expected<size_t, error> {
    auto &entry = cf_->dir.entry(dir_id_);

    uint64_t end = offset + buf.size();
    if (end > entry.stream_size) {
        auto r = resize(end);
        if (!r) {
            return std::unexpected(r.error());
        }
    }

    if (cfb::use_mini_stream(entry.stream_size)) {
        // Write to mini stream — walk mini FAT chain directly
        return cf_->with_sio([&](auto &sio) -> std::expected<size_t, error> {
            uint32_t mss = cf_->mini_sio.mini_sec_size();
            uint32_t skip = static_cast<uint32_t>(offset / mss);
            uint32_t off_in_sec = static_cast<uint32_t>(offset % mss);

            uint32_t cur = entry.start_sector;
            for (uint32_t i = 0; i < skip && cur != cfb::endofchain; ++i) {
                cur = cf_->mini_fat.next(cur);
            }

            size_t bytes_written = 0;
            uint8_t sec_buf[64];
            bool first = true;
            while (cur != cfb::endofchain && cur != cfb::freesect && bytes_written < buf.size()) {
                uint32_t start = first ? off_in_sec : 0;
                first = false;
                auto avail = mss - start;
                auto to_copy = std::min(static_cast<size_t>(avail), buf.size() - bytes_written);

                // If partial write, read first
                if (start != 0 || to_copy != mss) {
                    auto r = cf_->mini_sio.read_mini_sector(sio, cur, std::span<uint8_t>(sec_buf, mss));
                    if (!r) {
                        return std::unexpected(r.error());
                    }
                }
                std::copy_n(buf.data() + bytes_written, to_copy, sec_buf + start);
                auto w = cf_->mini_sio.write_mini_sector(sio, cur, std::span<const uint8_t>(sec_buf, mss));
                if (!w) {
                    return std::unexpected(w.error());
                }
                bytes_written += to_copy;
                cur = cf_->mini_fat.next(cur);
            }
            return bytes_written;
        });
    } else {
        // Write to regular sectors — walk FAT chain directly
        return cf_->with_sio([&](auto &sio) -> std::expected<size_t, error> {
            auto ss = cf_->sector_size();
            uint32_t skip = static_cast<uint32_t>(offset / ss);
            uint32_t off_in_sec = static_cast<uint32_t>(offset % ss);

            uint32_t cur = entry.start_sector;
            for (uint32_t i = 0; i < skip && cur != cfb::endofchain; ++i) {
                cur = cf_->fat.next(cur);
            }

            size_t bytes_written = 0;
            bool first = true;
            while (cur != cfb::endofchain && cur != cfb::freesect && bytes_written < buf.size()) {
                uint32_t start = first ? off_in_sec : 0;
                first = false;
                auto avail = ss - start;
                auto copy_len = std::min(static_cast<size_t>(avail), buf.size() - bytes_written);
                // Direct partial write — no intermediate buffer
                auto w = sio.write_at(cur, start, std::span<const uint8_t>(buf.data() + bytes_written, copy_len));
                if (!w) {
                    return std::unexpected(w.error());
                }
                bytes_written += copy_len;
                cur = cf_->fat.next(cur);
            }
            return bytes_written;
        });
    }
}

auto stream::resize(uint64_t new_size) -> std::expected<void, error> {
    auto &entry = cf_->dir.entry(dir_id_);
    auto old_size = entry.stream_size;

    if (new_size == old_size) {
        return {};
    }

    bool was_mini = cfb::use_mini_stream(old_size);
    bool will_be_mini = cfb::use_mini_stream(new_size);

    // Helper: find tail of a FAT chain (avoids heap alloc)
    auto find_tail = [](auto &fat_tbl, uint32_t start) -> uint32_t {
        uint32_t tail = start;
        while (tail != cfb::endofchain && tail != cfb::freesect) {
            auto nxt = fat_tbl.next(tail);
            if (nxt == cfb::endofchain || nxt == cfb::freesect) {
                break;
            }
            tail = nxt;
        }
        return tail;
    };

    // Helper: walk to nth sector in chain (0-indexed), returns sector id
    auto walk_to = [](auto &fat_tbl, uint32_t start, uint64_t n) -> uint32_t {
        uint32_t cur = start;
        for (uint64_t i = 0; i < n && cur != cfb::endofchain && cur != cfb::freesect; ++i) {
            cur = fat_tbl.next(cur);
        }
        return cur;
    };

    if (was_mini && will_be_mini) {
        auto old_count = (old_size + cfb::mini_sector_size - 1) / cfb::mini_sector_size;
        auto new_count = (new_size + cfb::mini_sector_size - 1) / cfb::mini_sector_size;

        if (new_count > old_count) {
            // Extend mini chain — find tail once, then append
            uint32_t tail = (entry.start_sector == cfb::endofchain) ? cfb::endofchain
                                                                    : find_tail(cf_->mini_fat, entry.start_sector);
            for (uint64_t i = old_count; i < new_count; ++i) {
                auto new_id = cf_->mini_fat.allocate();
                if (entry.start_sector == cfb::endofchain) {
                    entry.start_sector = new_id;
                    tail = new_id;
                } else {
                    cf_->mini_fat.set(tail, new_id);
                    tail = new_id;
                }
            }
            cf_->ensure_mini_stream_container(new_count);
        } else if (new_count < old_count) {
            if (new_count == 0) {
                cf_->mini_fat.free_chain(entry.start_sector);
                entry.start_sector = cfb::endofchain;
            } else {
                // Walk to the (new_count-1)th sector, then free the rest
                auto cut = walk_to(cf_->mini_fat, entry.start_sector, new_count - 1);
                auto rest = cf_->mini_fat.next(cut);
                cf_->mini_fat.set(cut, cfb::endofchain);
                if (rest != cfb::endofchain && rest != cfb::freesect) {
                    cf_->mini_fat.free_chain(rest);
                }
            }
        }
    } else if (!was_mini && !will_be_mini) {
        auto ss = cf_->sector_size();
        auto old_count = (old_size + ss - 1) / ss;
        auto new_count = (new_size + ss - 1) / ss;

        if (new_count > old_count) {
            uint32_t tail =
                (entry.start_sector == cfb::endofchain) ? cfb::endofchain : find_tail(cf_->fat, entry.start_sector);
            for (uint64_t i = old_count; i < new_count; ++i) {
                auto new_id = cf_->fat.allocate();
                if (entry.start_sector == cfb::endofchain) {
                    entry.start_sector = new_id;
                    tail = new_id;
                } else {
                    cf_->fat.set(tail, new_id);
                    tail = new_id;
                }
            }
        } else if (new_count < old_count) {
            if (new_count == 0) {
                cf_->fat.free_chain(entry.start_sector);
                entry.start_sector = cfb::endofchain;
            } else {
                auto cut = walk_to(cf_->fat, entry.start_sector, new_count - 1);
                auto rest = cf_->fat.next(cut);
                cf_->fat.set(cut, cfb::endofchain);
                if (rest != cfb::endofchain && rest != cfb::freesect) {
                    cf_->fat.free_chain(rest);
                }
            }
        }
    } else {
        // Transition between mini and regular — migrate data across storage types
        auto bytes_to_copy = std::min(old_size, new_size);
        std::vector<uint8_t> saved_data(static_cast<size_t>(bytes_to_copy));

        if (was_mini && !will_be_mini) {
            // Mini -> regular: read from mini stream, free mini chain, allocate regular, write data
            if (bytes_to_copy > 0) {
                // Read via the optimized read path (already uses chain walking)
                auto rd = read(0, std::span<uint8_t>(saved_data));
                if (!rd) {
                    return std::unexpected(rd.error());
                }
            }
            cf_->mini_fat.free_chain(entry.start_sector);
            entry.start_sector = cfb::endofchain;

            auto ss = cf_->sector_size();
            auto new_count = (new_size + ss - 1) / ss;
            uint32_t tail = cfb::endofchain;
            for (uint64_t i = 0; i < new_count; ++i) {
                auto new_id = cf_->fat.allocate();
                if (entry.start_sector == cfb::endofchain) {
                    entry.start_sector = new_id;
                    tail = new_id;
                } else {
                    cf_->fat.set(tail, new_id);
                    tail = new_id;
                }
            }

            // Write saved data to the new regular chain
            if (bytes_to_copy > 0) {
                entry.stream_size = new_size; // temporarily set so write sees correct type
                auto write_r = cf_->with_sio([&](auto &sio) -> std::expected<size_t, error> {
                    uint32_t cur = entry.start_sector;
                    size_t bytes_written = 0;
                    while (cur != cfb::endofchain && cur != cfb::freesect && bytes_written < bytes_to_copy) {
                        auto copy_len =
                            std::min(static_cast<size_t>(ss), static_cast<size_t>(bytes_to_copy) - bytes_written);
                        auto w =
                            sio.write_at(cur, 0, std::span<const uint8_t>(saved_data.data() + bytes_written, copy_len));
                        if (!w) {
                            return std::unexpected(w.error());
                        }
                        bytes_written += copy_len;
                        cur = cf_->fat.next(cur);
                    }
                    return bytes_written;
                });
                if (!write_r) {
                    return std::unexpected(write_r.error());
                }
            }
        } else {
            // Regular -> mini: read from regular sectors, free regular chain, allocate mini, write data
            if (bytes_to_copy > 0) {
                // Read via the optimized read path (already uses chain walking)
                auto rd = read(0, std::span<uint8_t>(saved_data));
                if (!rd) {
                    return std::unexpected(rd.error());
                }
            }
            cf_->fat.free_chain(entry.start_sector);
            entry.start_sector = cfb::endofchain;

            auto new_count = (new_size + cfb::mini_sector_size - 1) / cfb::mini_sector_size;
            uint32_t tail = cfb::endofchain;
            for (uint64_t i = 0; i < new_count; ++i) {
                auto new_id = cf_->mini_fat.allocate();
                if (entry.start_sector == cfb::endofchain) {
                    entry.start_sector = new_id;
                    tail = new_id;
                } else {
                    cf_->mini_fat.set(tail, new_id);
                    tail = new_id;
                }
            }
            cf_->ensure_mini_stream_container(new_count);

            // Write saved data to the new mini chain
            if (bytes_to_copy > 0) {
                entry.stream_size = new_size; // temporarily set so write sees correct type
                auto write_r = cf_->with_sio([&](auto &sio) -> std::expected<size_t, error> {
                    uint32_t mss = cf_->mini_sio.mini_sec_size();
                    uint32_t cur = entry.start_sector;
                    size_t bytes_written = 0;
                    uint8_t sec_buf[64];
                    while (cur != cfb::endofchain && cur != cfb::freesect && bytes_written < bytes_to_copy) {
                        auto to_copy =
                            std::min(static_cast<size_t>(mss), static_cast<size_t>(bytes_to_copy) - bytes_written);
                        if (to_copy < mss) {
                            std::memset(sec_buf, 0, mss);
                        }
                        std::copy_n(saved_data.data() + bytes_written, to_copy, sec_buf);
                        auto w = cf_->mini_sio.write_mini_sector(sio, cur, std::span<const uint8_t>(sec_buf, mss));
                        if (!w) {
                            return std::unexpected(w.error());
                        }
                        bytes_written += to_copy;
                        cur = cf_->mini_fat.next(cur);
                    }
                    return bytes_written;
                });
                if (!write_r) {
                    return std::unexpected(write_r.error());
                }
            }
        }
    }

    entry.stream_size = new_size;
    return {};
}

auto stream::rename(std::string_view new_name) -> std::expected<void, error> {
    if (new_name.empty()) {
        return std::unexpected(error::invalid_name);
    }
    if (new_name.size() > 31) {
        return std::unexpected(error::invalid_name);
    }
    auto u16 = util::utf8_to_utf16le(new_name);
    cf_->dir.entry(dir_id_).name = u16;
    return {};
}

auto stream::copy_to(stream &dest, uint64_t bytes) -> std::expected<uint64_t, error> {
    auto src_size = size();
    auto to_copy = std::min(bytes, src_size);
    if (to_copy == 0) {
        return uint64_t{0};
    }

    constexpr size_t chunk_size = 65536;
    uint64_t copied = 0;
    std::vector<uint8_t> buf(std::min(static_cast<size_t>(to_copy), chunk_size));

    // Ensure destination is large enough
    if (dest.size() < to_copy) {
        auto r = dest.resize(to_copy);
        if (!r) {
            return std::unexpected(r.error());
        }
    }

    while (copied < to_copy) {
        auto n = std::min(static_cast<size_t>(to_copy - copied), buf.size());
        auto rd = read(copied, std::span<uint8_t>(buf.data(), n));
        if (!rd) {
            return std::unexpected(rd.error());
        }
        if (*rd == 0) {
            break;
        }
        auto wr = dest.write(copied, std::span<const uint8_t>(buf.data(), *rd));
        if (!wr) {
            return std::unexpected(wr.error());
        }
        copied += *rd;
    }
    return copied;
}

} // namespace stout
