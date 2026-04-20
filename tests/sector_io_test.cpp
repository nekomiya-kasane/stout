#include "stout/cfb/header.h"
#include "stout/cfb/sector_io.h"
#include "stout/io/memory_lock_bytes.h"

#include <array>
#include <gtest/gtest.h>

using namespace stout;
using namespace stout::cfb;
using namespace stout::io;

TEST(SectorIOTest, SectorOffset) {
    EXPECT_EQ(sector_offset(0, 512), 512u);
    EXPECT_EQ(sector_offset(1, 512), 1024u);
    EXPECT_EQ(sector_offset(0, 4096), 4096u);
    EXPECT_EQ(sector_offset(1, 4096), 8192u);
}

TEST(SectorIOTest, WriteAndReadSector) {
    memory_lock_bytes mlb;
    mlb.set_size(512 + 512); // header + 1 sector
    sector_io sio(mlb, 512);

    std::array<uint8_t, 512> write_data;
    write_data.fill(0xAB);
    auto wr = sio.write_sector(0, write_data);
    ASSERT_TRUE(wr.has_value());

    std::array<uint8_t, 512> read_data = {};
    auto rd = sio.read_sector(0, read_data);
    ASSERT_TRUE(rd.has_value());
    EXPECT_EQ(read_data, write_data);
}

TEST(SectorIOTest, WriteAndReadHeader) {
    memory_lock_bytes mlb;
    mlb.set_size(512);

    auto hdr = make_default_header(cfb_version::v3);
    std::array<uint8_t, 512> buf = {};
    serialize_header(hdr, buf);

    sector_io sio(mlb, 512);
    auto wr = sio.write_header(buf);
    ASSERT_TRUE(wr.has_value());

    std::array<uint8_t, 512> read_buf = {};
    auto rd = sio.read_header(read_buf);
    ASSERT_TRUE(rd.has_value());
    EXPECT_EQ(read_buf, buf);
}

TEST(SectorIOTest, MultipleSectors) {
    memory_lock_bytes mlb;
    mlb.set_size(512 + 512 * 3); // header + 3 sectors
    sector_io sio(mlb, 512);

    for (uint32_t i = 0; i < 3; ++i) {
        std::array<uint8_t, 512> data;
        data.fill(static_cast<uint8_t>(i + 1));
        auto wr = sio.write_sector(i, data);
        ASSERT_TRUE(wr.has_value());
    }

    for (uint32_t i = 0; i < 3; ++i) {
        std::array<uint8_t, 512> data = {};
        auto rd = sio.read_sector(i, data);
        ASSERT_TRUE(rd.has_value());
        EXPECT_EQ(data[0], static_cast<uint8_t>(i + 1));
        EXPECT_EQ(data[511], static_cast<uint8_t>(i + 1));
    }
}

TEST(SectorIOTest, V4SectorSize) {
    memory_lock_bytes mlb;
    mlb.set_size(4096 + 4096); // header + 1 sector (v4 header is padded to 4096)
    sector_io sio(mlb, 4096);

    std::array<uint8_t, 4096> write_data;
    write_data.fill(0xCD);
    auto wr = sio.write_sector(0, write_data);
    ASSERT_TRUE(wr.has_value());

    std::array<uint8_t, 4096> read_data = {};
    auto rd = sio.read_sector(0, read_data);
    ASSERT_TRUE(rd.has_value());
    EXPECT_EQ(read_data, write_data);
}

TEST(SectorIOTest, ReadAtPartial) {
    memory_lock_bytes mlb;
    mlb.set_size(512 + 512);
    sector_io sio(mlb, 512);

    std::array<uint8_t, 512> data;
    for (uint32_t i = 0; i < 512; ++i) data[i] = static_cast<uint8_t>(i & 0xFF);
    sio.write_sector(0, data);

    std::array<uint8_t, 4> partial = {};
    auto result = sio.read_at(0, 100, partial);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 4u);
    EXPECT_EQ(partial[0], 100);
    EXPECT_EQ(partial[1], 101);
}

TEST(SectorIOTest, BufferTooSmall) {
    memory_lock_bytes mlb;
    mlb.set_size(512 + 512);
    sector_io sio(mlb, 512);

    std::array<uint8_t, 100> small_buf = {};
    auto result = sio.read_sector(0, small_buf);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), error::invalid_argument);
}

TEST(SectorIOTest, Flush) {
    memory_lock_bytes mlb;
    sector_io sio(mlb, 512);
    EXPECT_TRUE(sio.flush().has_value());
}
