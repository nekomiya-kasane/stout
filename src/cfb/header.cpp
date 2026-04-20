#include "stout/cfb/header.h"
#include "stout/util/endian.h"
#include <algorithm>
#include <cstring>

namespace stout::cfb {

auto make_default_header(cfb_version version) noexcept -> cfb_header {
    cfb_header hdr;
    hdr.signature = cfb::signature;
    hdr.clsid = guid_null;
    hdr.minor_ver = minor_version;
    hdr.major_ver = (version == cfb_version::v3) ? major_version_3 : major_version_4;
    hdr.byte_order = byte_order_le;
    hdr.sector_shift = (version == cfb_version::v3) ? sector_shift_v3 : sector_shift_v4;
    hdr.mini_sec_shift = mini_sector_shift;
    hdr.total_dir_sectors = 0;
    hdr.total_fat_sectors = 0;
    hdr.first_dir_sector = endofchain;
    hdr.transaction_sig = 0;
    hdr.mini_stream_cutoff_size = mini_stream_cutoff;
    hdr.first_mini_fat_sector = endofchain;
    hdr.total_mini_fat_sectors = 0;
    hdr.first_difat_sector = endofchain;
    hdr.total_difat_sectors = 0;
    hdr.difat.fill(freesect);
    return hdr;
}

auto parse_header(std::span<const uint8_t, header_size> d) noexcept
    -> std::expected<cfb_header, error> {
    using namespace util;

    cfb_header hdr;

    // Signature (offset 0, 8 bytes)
    std::copy_n(d.data(), 8, hdr.signature.begin());
    if (hdr.signature != cfb::signature)
        return std::unexpected(error::invalid_signature);

    // CLSID (offset 8, 16 bytes)
    hdr.clsid = guid_read_le(d.data() + 8);

    // Minor version (offset 24, 2 bytes)
    hdr.minor_ver = read_u16_le(d.data() + 24);

    // Major version (offset 26, 2 bytes)
    hdr.major_ver = read_u16_le(d.data() + 26);

    // Byte order (offset 28, 2 bytes)
    hdr.byte_order = read_u16_le(d.data() + 28);
    if (hdr.byte_order != byte_order_le)
        return std::unexpected(error::invalid_header);

    // Sector shift (offset 30, 2 bytes)
    hdr.sector_shift = read_u16_le(d.data() + 30);

    // Mini sector shift (offset 32, 2 bytes)
    hdr.mini_sec_shift = read_u16_le(d.data() + 32);

    // Reserved (offset 34, 6 bytes) — skip

    // Total directory sectors (offset 40, 4 bytes) — must be 0 for v3
    hdr.total_dir_sectors = read_u32_le(d.data() + 40);

    // Total FAT sectors (offset 44, 4 bytes)
    hdr.total_fat_sectors = read_u32_le(d.data() + 44);

    // First directory sector (offset 48, 4 bytes)
    hdr.first_dir_sector = read_u32_le(d.data() + 48);

    // Transaction signature (offset 52, 4 bytes)
    hdr.transaction_sig = read_u32_le(d.data() + 52);

    // Mini stream cutoff size (offset 56, 4 bytes)
    hdr.mini_stream_cutoff_size = read_u32_le(d.data() + 56);

    // First mini FAT sector (offset 60, 4 bytes)
    hdr.first_mini_fat_sector = read_u32_le(d.data() + 60);

    // Total mini FAT sectors (offset 64, 4 bytes)
    hdr.total_mini_fat_sectors = read_u32_le(d.data() + 64);

    // First DIFAT sector (offset 68, 4 bytes)
    hdr.first_difat_sector = read_u32_le(d.data() + 68);

    // Total DIFAT sectors (offset 72, 4 bytes)
    hdr.total_difat_sectors = read_u32_le(d.data() + 72);

    // DIFAT array (offset 76, 109 * 4 = 436 bytes)
    for (uint32_t i = 0; i < difat_in_header; ++i) {
        hdr.difat[i] = read_u32_le(d.data() + 76 + i * 4);
    }

    return hdr;
}

void serialize_header(const cfb_header& hdr, std::span<uint8_t, header_size> out) noexcept {
    using namespace util;

    std::fill(out.begin(), out.end(), uint8_t{0});

    // Signature
    std::copy_n(hdr.signature.begin(), 8, out.data());

    // CLSID
    guid_write_le(out.data() + 8, hdr.clsid);

    // Minor/Major version
    write_u16_le(out.data() + 24, hdr.minor_ver);
    write_u16_le(out.data() + 26, hdr.major_ver);

    // Byte order
    write_u16_le(out.data() + 28, hdr.byte_order);

    // Sector shift
    write_u16_le(out.data() + 30, hdr.sector_shift);

    // Mini sector shift
    write_u16_le(out.data() + 32, hdr.mini_sec_shift);

    // Reserved (34-39) — already zero

    // Total directory sectors
    write_u32_le(out.data() + 40, hdr.total_dir_sectors);

    // Total FAT sectors
    write_u32_le(out.data() + 44, hdr.total_fat_sectors);

    // First directory sector
    write_u32_le(out.data() + 48, hdr.first_dir_sector);

    // Transaction signature
    write_u32_le(out.data() + 52, hdr.transaction_sig);

    // Mini stream cutoff size
    write_u32_le(out.data() + 56, hdr.mini_stream_cutoff_size);

    // First mini FAT sector
    write_u32_le(out.data() + 60, hdr.first_mini_fat_sector);

    // Total mini FAT sectors
    write_u32_le(out.data() + 64, hdr.total_mini_fat_sectors);

    // First DIFAT sector
    write_u32_le(out.data() + 68, hdr.first_difat_sector);

    // Total DIFAT sectors
    write_u32_le(out.data() + 72, hdr.total_difat_sectors);

    // DIFAT array
    for (uint32_t i = 0; i < difat_in_header; ++i) {
        write_u32_le(out.data() + 76 + i * 4, hdr.difat[i]);
    }
}

auto validate_header(const cfb_header& hdr) noexcept -> std::expected<void, error> {
    if (hdr.signature != cfb::signature)
        return std::unexpected(error::invalid_signature);

    if (hdr.major_ver != major_version_3 && hdr.major_ver != major_version_4)
        return std::unexpected(error::invalid_version);

    if (hdr.byte_order != byte_order_le)
        return std::unexpected(error::invalid_header);

    if (hdr.major_ver == major_version_3 && hdr.sector_shift != sector_shift_v3)
        return std::unexpected(error::invalid_sector_size);

    if (hdr.major_ver == major_version_4 && hdr.sector_shift != sector_shift_v4)
        return std::unexpected(error::invalid_sector_size);

    if (hdr.mini_sec_shift != mini_sector_shift)
        return std::unexpected(error::invalid_header);

    if (hdr.major_ver == major_version_3 && hdr.total_dir_sectors != 0)
        return std::unexpected(error::invalid_header);

    if (hdr.mini_stream_cutoff_size != mini_stream_cutoff)
        return std::unexpected(error::invalid_header);

    return {};
}

} // namespace stout::cfb
