#include "stout/cfb/fat.h"
#include "stout/io/memory_lock_bytes.h"

#include <gtest/gtest.h>
#include <numeric>

using namespace stout;
using namespace stout::cfb;
using namespace stout::io;

TEST(FatTableTest, EmptyTable) {
    fat_table fat;
    EXPECT_EQ(fat.size(), 0u);
    EXPECT_EQ(fat.next(0), freesect);
}

TEST(FatTableTest, SetAndGet) {
    fat_table fat;
    fat.resize(10);
    fat.set(0, 1);
    fat.set(1, 2);
    fat.set(2, endofchain);
    EXPECT_EQ(fat.next(0), 1u);
    EXPECT_EQ(fat.next(1), 2u);
    EXPECT_EQ(fat.next(2), endofchain);
    EXPECT_EQ(fat.next(3), freesect);
}

TEST(FatTableTest, SetAutoResize) {
    fat_table fat;
    fat.set(100, endofchain);
    EXPECT_GE(fat.size(), 101u);
    EXPECT_EQ(fat.next(100), endofchain);
}

TEST(FatTableTest, Allocate) {
    fat_table fat;
    fat.resize(5);
    // All free initially
    auto id = fat.allocate();
    EXPECT_EQ(id, 0u);
    EXPECT_EQ(fat.next(0), endofchain);

    auto id2 = fat.allocate();
    EXPECT_EQ(id2, 1u);
}

TEST(FatTableTest, AllocateSkipsUsed) {
    fat_table fat;
    fat.resize(5);
    fat.set(0, endofchain);
    fat.set(1, endofchain);
    auto id = fat.allocate();
    EXPECT_EQ(id, 2u);
}

TEST(FatTableTest, AllocateGrows) {
    fat_table fat;
    fat.resize(2);
    fat.set(0, endofchain);
    fat.set(1, endofchain);
    auto id = fat.allocate();
    EXPECT_EQ(id, 2u);
    EXPECT_GE(fat.size(), 3u);
}

TEST(FatTableTest, FreeSector) {
    fat_table fat;
    fat.resize(5);
    fat.set(2, endofchain);
    EXPECT_EQ(fat.next(2), endofchain);
    fat.free_sector(2);
    EXPECT_EQ(fat.next(2), freesect);
}

TEST(FatTableTest, Chain) {
    fat_table fat;
    fat.resize(10);
    fat.set(0, 3);
    fat.set(3, 7);
    fat.set(7, endofchain);

    auto c = fat.chain(0);
    ASSERT_EQ(c.size(), 3u);
    EXPECT_EQ(c[0], 0u);
    EXPECT_EQ(c[1], 3u);
    EXPECT_EQ(c[2], 7u);
}

TEST(FatTableTest, ChainSingle) {
    fat_table fat;
    fat.resize(5);
    fat.set(2, endofchain);
    auto c = fat.chain(2);
    ASSERT_EQ(c.size(), 1u);
    EXPECT_EQ(c[0], 2u);
}

TEST(FatTableTest, ChainEmpty) {
    fat_table fat;
    auto c = fat.chain(endofchain);
    EXPECT_TRUE(c.empty());
}

TEST(FatTableTest, FreeChain) {
    fat_table fat;
    fat.resize(10);
    fat.set(0, 3);
    fat.set(3, 7);
    fat.set(7, endofchain);

    fat.free_chain(0);
    EXPECT_EQ(fat.next(0), freesect);
    EXPECT_EQ(fat.next(3), freesect);
    EXPECT_EQ(fat.next(7), freesect);
}

TEST(FatTableTest, ExtendChain) {
    fat_table fat;
    fat.resize(10);
    fat.set(0, 3);
    fat.set(3, endofchain);

    auto new_id = fat.extend_chain(0);
    // Chain should now be 0 -> 3 -> new_id -> ENDOFCHAIN
    EXPECT_EQ(fat.next(3), new_id);
    EXPECT_EQ(fat.next(new_id), endofchain);
}

TEST(FatTableTest, ExtendEmptyChain) {
    fat_table fat;
    fat.resize(5);
    auto new_id = fat.extend_chain(endofchain);
    EXPECT_EQ(fat.next(new_id), endofchain);
}

// ── Sector chain iterator tests ────────────────────────────────────────

TEST(SectorChainIteratorTest, IterateChain) {
    fat_table fat;
    fat.resize(10);
    fat.set(0, 3);
    fat.set(3, 7);
    fat.set(7, endofchain);

    std::vector<uint32_t> collected;
    for (auto id : iterate_chain(fat, 0)) {
        collected.push_back(id);
    }
    ASSERT_EQ(collected.size(), 3u);
    EXPECT_EQ(collected[0], 0u);
    EXPECT_EQ(collected[1], 3u);
    EXPECT_EQ(collected[2], 7u);
}

TEST(SectorChainIteratorTest, EmptyChain) {
    fat_table fat;
    std::vector<uint32_t> collected;
    for (auto id : iterate_chain(fat, endofchain)) {
        collected.push_back(id);
    }
    EXPECT_TRUE(collected.empty());
}

TEST(SectorChainIteratorTest, SingleSector) {
    fat_table fat;
    fat.resize(5);
    fat.set(2, endofchain);

    std::vector<uint32_t> collected;
    for (auto id : iterate_chain(fat, 2)) {
        collected.push_back(id);
    }
    ASSERT_EQ(collected.size(), 1u);
    EXPECT_EQ(collected[0], 2u);
}

// ── FAT load/flush with sector_io ──────────────────────────────────────

TEST(FatTableTest, LoadFlushRoundtrip) {
    memory_lock_bytes mlb;
    // Header (512) + 1 FAT sector (512) = 1024
    mlb.set_size(1024);
    sector_io sio(mlb, 512);

    // Build a FAT in memory
    fat_table fat_orig;
    fat_orig.resize(128);     // 512/4 = 128 entries per sector
    fat_orig.set(0, fatsect); // sector 0 is the FAT sector itself
    fat_orig.set(1, 2);
    fat_orig.set(2, 3);
    fat_orig.set(3, endofchain);

    // Flush to sector 0
    std::array<uint32_t, 1> fat_sectors = {0};
    auto wr = fat_orig.flush(sio, std::span<const uint32_t>{fat_sectors});
    ASSERT_TRUE(wr.has_value());

    // Load back
    fat_table fat_loaded;
    auto rd = fat_loaded.load(sio, std::span<const uint32_t>{fat_sectors});
    ASSERT_TRUE(rd.has_value());

    EXPECT_EQ(fat_loaded.next(0), fatsect);
    EXPECT_EQ(fat_loaded.next(1), 2u);
    EXPECT_EQ(fat_loaded.next(2), 3u);
    EXPECT_EQ(fat_loaded.next(3), endofchain);
    EXPECT_EQ(fat_loaded.next(4), freesect);
}

TEST(FatTableTest, LoadMultipleSectors) {
    memory_lock_bytes mlb;
    // Header (512) + 2 FAT sectors (1024) = 1536
    mlb.set_size(1536);
    sector_io sio(mlb, 512);

    fat_table fat_orig;
    fat_orig.resize(256); // 2 sectors * 128 entries
    fat_orig.set(0, fatsect);
    fat_orig.set(1, fatsect);
    fat_orig.set(2, 3);
    fat_orig.set(3, endofchain);
    fat_orig.set(130, 131);
    fat_orig.set(131, endofchain);

    std::array<uint32_t, 2> fat_sectors = {0, 1};
    auto wr = fat_orig.flush(sio, std::span<const uint32_t>{fat_sectors});
    ASSERT_TRUE(wr.has_value());

    fat_table fat_loaded;
    auto rd = fat_loaded.load(sio, std::span<const uint32_t>{fat_sectors});
    ASSERT_TRUE(rd.has_value());

    EXPECT_EQ(fat_loaded.next(0), fatsect);
    EXPECT_EQ(fat_loaded.next(1), fatsect);
    EXPECT_EQ(fat_loaded.next(2), 3u);
    EXPECT_EQ(fat_loaded.next(3), endofchain);
    EXPECT_EQ(fat_loaded.next(130), 131u);
    EXPECT_EQ(fat_loaded.next(131), endofchain);
}
