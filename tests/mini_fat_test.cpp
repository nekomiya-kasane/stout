#include <gtest/gtest.h>
#include "stout/cfb/mini_fat.h"
#include "stout/io/memory_lock_bytes.h"

using namespace stout;
using namespace stout::cfb;
using namespace stout::io;

// ── mini_fat_table tests ───────────────────────────────────────────────

TEST(MiniFatTableTest, EmptyTable) {
    mini_fat_table mfat;
    EXPECT_EQ(mfat.size(), 0u);
    EXPECT_EQ(mfat.next(0), freesect);
}

TEST(MiniFatTableTest, SetAndGet) {
    mini_fat_table mfat;
    mfat.resize(10);
    mfat.set(0, 1);
    mfat.set(1, 2);
    mfat.set(2, endofchain);
    EXPECT_EQ(mfat.next(0), 1u);
    EXPECT_EQ(mfat.next(1), 2u);
    EXPECT_EQ(mfat.next(2), endofchain);
    EXPECT_EQ(mfat.next(3), freesect);
}

TEST(MiniFatTableTest, Allocate) {
    mini_fat_table mfat;
    mfat.resize(5);
    auto id = mfat.allocate();
    EXPECT_EQ(id, 0u);
    EXPECT_EQ(mfat.next(0), endofchain);
}

TEST(MiniFatTableTest, FreeSector) {
    mini_fat_table mfat;
    mfat.resize(5);
    mfat.set(2, endofchain);
    mfat.free_sector(2);
    EXPECT_EQ(mfat.next(2), freesect);
}

TEST(MiniFatTableTest, Chain) {
    mini_fat_table mfat;
    mfat.resize(10);
    mfat.set(0, 3);
    mfat.set(3, 7);
    mfat.set(7, endofchain);
    auto c = mfat.chain(0);
    ASSERT_EQ(c.size(), 3u);
    EXPECT_EQ(c[0], 0u);
    EXPECT_EQ(c[1], 3u);
    EXPECT_EQ(c[2], 7u);
}

TEST(MiniFatTableTest, FreeChain) {
    mini_fat_table mfat;
    mfat.resize(10);
    mfat.set(0, 3);
    mfat.set(3, endofchain);
    mfat.free_chain(0);
    EXPECT_EQ(mfat.next(0), freesect);
    EXPECT_EQ(mfat.next(3), freesect);
}

TEST(MiniFatTableTest, LoadFlushRoundtrip) {
    memory_lock_bytes mlb;
    uint32_t ss = 512;
    // header + 2 regular sectors (sector 0 = mini FAT, sector 1 = something)
    mlb.set_size(ss * 3);
    sector_io sio(mlb, ss);

    // Build a FAT: sector 0 holds mini FAT data, chain = [0] -> ENDOFCHAIN
    fat_table fat;
    fat.resize(2);
    fat.set(0, endofchain); // mini FAT sector
    fat.set(1, endofchain);

    // Build a mini FAT in memory
    mini_fat_table mfat_orig;
    mfat_orig.resize(128); // 512/4 = 128 entries
    mfat_orig.set(0, 1);
    mfat_orig.set(1, 2);
    mfat_orig.set(2, endofchain);

    // Flush to sector 0
    auto wr = mfat_orig.flush(sio, fat, 0);
    ASSERT_TRUE(wr.has_value());

    // Load back
    mini_fat_table mfat_loaded;
    auto rd = mfat_loaded.load(sio, fat, 0);
    ASSERT_TRUE(rd.has_value());

    EXPECT_EQ(mfat_loaded.next(0), 1u);
    EXPECT_EQ(mfat_loaded.next(1), 2u);
    EXPECT_EQ(mfat_loaded.next(2), endofchain);
    EXPECT_EQ(mfat_loaded.next(3), freesect);
}

TEST(MiniFatTableTest, LoadEmptyChain) {
    memory_lock_bytes mlb;
    mlb.set_size(1024);
    sector_io sio(mlb, 512);
    fat_table fat;

    mini_fat_table mfat;
    auto r = mfat.load(sio, fat, endofchain);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(mfat.size(), 0u);
}

// ── mini_stream_io tests ───────────────────────────────────────────────

TEST(MiniStreamIOTest, ReadWriteMiniSector) {
    memory_lock_bytes mlb;
    uint32_t ss = 512;
    // header (512) + 1 regular sector (512) for mini stream container
    mlb.set_size(ss * 2);
    sector_io sio(mlb, ss);

    // Root chain: sector 0 holds the mini stream container
    mini_stream_io msio;
    msio.init({0}, ss, 64);

    // Write 64 bytes to mini sector 0
    std::vector<uint8_t> write_data(64, 0xAB);
    auto wr = msio.write_mini_sector(sio, 0, write_data);
    ASSERT_TRUE(wr.has_value());

    // Read back
    std::vector<uint8_t> read_data(64, 0);
    auto rd = msio.read_mini_sector(sio, 0, read_data);
    ASSERT_TRUE(rd.has_value());
    EXPECT_EQ(read_data, write_data);
}

TEST(MiniStreamIOTest, MultipleMiniSectors) {
    memory_lock_bytes mlb;
    uint32_t ss = 512;
    // 512 bytes per sector / 64 bytes per mini sector = 8 mini sectors per regular sector
    mlb.set_size(ss * 2);
    sector_io sio(mlb, ss);

    mini_stream_io msio;
    msio.init({0}, ss, 64);

    // Write to mini sectors 0..7
    for (uint32_t i = 0; i < 8; ++i) {
        std::vector<uint8_t> data(64, static_cast<uint8_t>(i + 1));
        auto wr = msio.write_mini_sector(sio, i, data);
        ASSERT_TRUE(wr.has_value());
    }

    // Read back and verify
    for (uint32_t i = 0; i < 8; ++i) {
        std::vector<uint8_t> data(64, 0);
        auto rd = msio.read_mini_sector(sio, i, data);
        ASSERT_TRUE(rd.has_value());
        EXPECT_EQ(data[0], static_cast<uint8_t>(i + 1));
        EXPECT_EQ(data[63], static_cast<uint8_t>(i + 1));
    }
}

TEST(MiniStreamIOTest, MiniSectorAcrossRegularSectors) {
    memory_lock_bytes mlb;
    uint32_t ss = 512;
    // 2 regular sectors for mini stream container = 16 mini sectors
    mlb.set_size(ss * 3);
    sector_io sio(mlb, ss);

    mini_stream_io msio;
    msio.init({0, 1}, ss, 64);

    // Write to mini sector 9 (in second regular sector)
    std::vector<uint8_t> data(64, 0xCD);
    auto wr = msio.write_mini_sector(sio, 9, data);
    ASSERT_TRUE(wr.has_value());

    std::vector<uint8_t> buf(64, 0);
    auto rd = msio.read_mini_sector(sio, 9, buf);
    ASSERT_TRUE(rd.has_value());
    EXPECT_EQ(buf, data);
}

TEST(MiniStreamIOTest, ReadMiniStream) {
    memory_lock_bytes mlb;
    uint32_t ss = 512;
    mlb.set_size(ss * 2);
    sector_io sio(mlb, ss);

    mini_stream_io msio;
    msio.init({0}, ss, 64);

    // Set up mini FAT: chain 0 -> 1 -> 2 -> ENDOFCHAIN
    mini_fat_table mfat;
    mfat.resize(8);
    mfat.set(0, 1);
    mfat.set(1, 2);
    mfat.set(2, endofchain);

    // Write known data to mini sectors 0, 1, 2
    for (uint32_t i = 0; i < 3; ++i) {
        std::vector<uint8_t> data(64, static_cast<uint8_t>(i + 0x10));
        msio.write_mini_sector(sio, i, data);
    }

    // Read 100 bytes from offset 0 of the mini stream
    std::vector<uint8_t> buf(100, 0);
    auto result = msio.read_mini_stream(sio, mfat, 0, 0, buf);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 100u);
    // First 64 bytes should be 0x10
    EXPECT_EQ(buf[0], 0x10);
    EXPECT_EQ(buf[63], 0x10);
    // Next 36 bytes should be 0x11
    EXPECT_EQ(buf[64], 0x11);
    EXPECT_EQ(buf[99], 0x11);
}

TEST(MiniStreamIOTest, ReadMiniStreamWithOffset) {
    memory_lock_bytes mlb;
    uint32_t ss = 512;
    mlb.set_size(ss * 2);
    sector_io sio(mlb, ss);

    mini_stream_io msio;
    msio.init({0}, ss, 64);

    mini_fat_table mfat;
    mfat.resize(8);
    mfat.set(0, 1);
    mfat.set(1, endofchain);

    // Write data
    std::vector<uint8_t> d0(64, 0xAA);
    std::vector<uint8_t> d1(64, 0xBB);
    msio.write_mini_sector(sio, 0, d0);
    msio.write_mini_sector(sio, 1, d1);

    // Read 10 bytes starting at offset 60 (crosses mini sector boundary)
    std::vector<uint8_t> buf(10, 0);
    auto result = msio.read_mini_stream(sio, mfat, 0, 60, buf);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 10u);
    // First 4 bytes from sector 0
    EXPECT_EQ(buf[0], 0xAA);
    EXPECT_EQ(buf[3], 0xAA);
    // Next 6 bytes from sector 1
    EXPECT_EQ(buf[4], 0xBB);
    EXPECT_EQ(buf[9], 0xBB);
}

TEST(MiniStreamIOTest, UseMiniStream) {
    EXPECT_TRUE(use_mini_stream(0));
    EXPECT_TRUE(use_mini_stream(100));
    EXPECT_TRUE(use_mini_stream(4095));
    EXPECT_FALSE(use_mini_stream(4096));
    EXPECT_FALSE(use_mini_stream(10000));
}
