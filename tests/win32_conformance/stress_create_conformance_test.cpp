#ifdef _WIN32

#    include "conformance_utils.h"

#    include <algorithm>
#    include <cstring>
#    include <gtest/gtest.h>
#    include <stout/cfb/constants.h>
#    include <stout/compound_file.h>

using namespace conformance;
using namespace stout;

// ── Parameterized over v3/v4 ────────────────────────────────────────────

struct VersionParam {
    cfb_version ver;
    uint16_t major;
    uint32_t sector_size;
    uint16_t sector_shift;
};

class StressCreateConformance : public ::testing::TestWithParam<VersionParam> {
  protected:
    com_init com_;
    temp_file_guard guard_;
};

static const VersionParam versions[] = {
    {cfb_version::v3, 3, 512, 9},
    {cfb_version::v4, 4, 4096, 12},
};

INSTANTIATE_TEST_SUITE_P(V, StressCreateConformance, ::testing::ValuesIn(versions),
                         [](const auto &info) { return info.param.major == 3 ? "V3" : "V4"; });

// ── Basic create + Win32 open ───────────────────────────────────────────

TEST_P(StressCreateConformance, EmptyFile_StoutCreateWin32Open) {
    auto path = temp_file("sc_empty");
    guard_.add(path);
    {
        auto cf = compound_file::create(path, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
}

TEST_P(StressCreateConformance, EmptyFile_Win32CreateStoutOpen) {
    auto path = temp_file("sc_w32empty");
    guard_.add(path);
    {
        storage_ptr stg;
        if (GetParam().ver == cfb_version::v4) {
            ASSERT_TRUE(SUCCEEDED(win32_create_v4(path.wstring(), stg.put())));
        } else {
            ASSERT_TRUE(SUCCEEDED(win32_create_v3(path.wstring(), stg.put())));
        }
    }
    auto cf = compound_file::open(path, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    EXPECT_EQ(cf->version(), GetParam().ver);
}

TEST_P(StressCreateConformance, CreateFlushReopenStout) {
    auto path = temp_file("sc_reopen");
    guard_.add(path);
    {
        auto cf = compound_file::create(path, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(path, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    EXPECT_EQ(cf->version(), GetParam().ver);
    EXPECT_EQ(cf->root_storage().children().size(), 0u);
}

// ── Header byte-level checks ────────────────────────────────────────────

TEST_P(StressCreateConformance, HeaderSignatureBytes) {
    auto path = temp_file("sc_sig");
    guard_.add(path);
    {
        auto cf = compound_file::create(path, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
    }
    auto bytes = read_file_bytes(path);
    ASSERT_GE(bytes.size(), 512u);
    EXPECT_EQ(bytes[0], 0xD0);
    EXPECT_EQ(bytes[1], 0xCF);
    EXPECT_EQ(bytes[2], 0x11);
    EXPECT_EQ(bytes[3], 0xE0);
    EXPECT_EQ(bytes[4], 0xA1);
    EXPECT_EQ(bytes[5], 0xB1);
    EXPECT_EQ(bytes[6], 0x1A);
    EXPECT_EQ(bytes[7], 0xE1);
}

TEST_P(StressCreateConformance, HeaderMajorVersion) {
    auto path = temp_file("sc_major");
    guard_.add(path);
    {
        auto cf = compound_file::create(path, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
    }
    auto bytes = read_file_bytes(path);
    uint16_t major = bytes[0x1A] | (bytes[0x1B] << 8);
    EXPECT_EQ(major, GetParam().major);
}

TEST_P(StressCreateConformance, HeaderMinorVersion) {
    auto path = temp_file("sc_minor");
    guard_.add(path);
    {
        auto cf = compound_file::create(path, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
    }
    auto bytes = read_file_bytes(path);
    uint16_t minor = bytes[0x18] | (bytes[0x19] << 8);
    EXPECT_EQ(minor, 0x003Eu);
}

TEST_P(StressCreateConformance, HeaderByteOrder) {
    auto path = temp_file("sc_bom");
    guard_.add(path);
    {
        auto cf = compound_file::create(path, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
    }
    auto bytes = read_file_bytes(path);
    uint16_t bom = bytes[0x1C] | (bytes[0x1D] << 8);
    EXPECT_EQ(bom, 0xFFFEu);
}

TEST_P(StressCreateConformance, HeaderSectorShift) {
    auto path = temp_file("sc_shift");
    guard_.add(path);
    {
        auto cf = compound_file::create(path, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
    }
    auto bytes = read_file_bytes(path);
    uint16_t shift = bytes[0x1E] | (bytes[0x1F] << 8);
    EXPECT_EQ(shift, GetParam().sector_shift);
}

TEST_P(StressCreateConformance, HeaderMiniSectorShift) {
    auto path = temp_file("sc_minishift");
    guard_.add(path);
    {
        auto cf = compound_file::create(path, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
    }
    auto bytes = read_file_bytes(path);
    uint16_t shift = bytes[0x20] | (bytes[0x21] << 8);
    EXPECT_EQ(shift, 6u);
}

TEST_P(StressCreateConformance, HeaderReservedZeros) {
    auto path = temp_file("sc_reserved");
    guard_.add(path);
    {
        auto cf = compound_file::create(path, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
    }
    auto bytes = read_file_bytes(path);
    // Reserved bytes at offset 0x22 (6 bytes) should be zero
    for (int i = 0x22; i < 0x28; ++i) {
        EXPECT_EQ(bytes[i], 0u) << "Non-zero at offset 0x" << std::hex << i;
    }
}

TEST_P(StressCreateConformance, HeaderMatchesWin32) {
    auto [stout_path, win32_path] = temp_file_pair("sc_hdrmatch");
    guard_.add(stout_path);
    guard_.add(win32_path);
    {
        auto cf = compound_file::create(stout_path, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    {
        storage_ptr stg;
        if (GetParam().ver == cfb_version::v4) {
            ASSERT_TRUE(SUCCEEDED(win32_create_v4(win32_path.wstring(), stg.put())));
        } else {
            ASSERT_TRUE(SUCCEEDED(win32_create_v3(win32_path.wstring(), stg.put())));
        }
    }
    auto sb = read_file_bytes(stout_path);
    auto wb = read_file_bytes(win32_path);
    // Signature must match
    EXPECT_TRUE(std::equal(sb.begin(), sb.begin() + 8, wb.begin()));
    // Major version must match
    EXPECT_EQ(sb[0x1A], wb[0x1A]);
    EXPECT_EQ(sb[0x1B], wb[0x1B]);
    // Sector shift must match
    EXPECT_EQ(sb[0x1E], wb[0x1E]);
    EXPECT_EQ(sb[0x1F], wb[0x1F]);
}

// ── File size checks ────────────────────────────────────────────────────

TEST_P(StressCreateConformance, FileSizeAligned) {
    auto path = temp_file("sc_align");
    guard_.add(path);
    {
        auto cf = compound_file::create(path, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto sz = std::filesystem::file_size(path);
    EXPECT_GE(sz, static_cast<uintmax_t>(GetParam().sector_size));
    EXPECT_EQ(sz % GetParam().sector_size, 0u);
}

TEST_P(StressCreateConformance, FileSizeMinimum) {
    auto path = temp_file("sc_minsz");
    guard_.add(path);
    {
        auto cf = compound_file::create(path, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
    }
    auto sz = std::filesystem::file_size(path);
    // At minimum: header + FAT sector + directory sector
    EXPECT_GE(sz, static_cast<uintmax_t>(GetParam().sector_size * 2));
}

// ── Root entry checks ───────────────────────────────────────────────────

TEST_P(StressCreateConformance, RootClsidNull) {
    auto path = temp_file("sc_rclsid");
    guard_.add(path);
    {
        auto cf = compound_file::create(path, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    STATSTG st{};
    ASSERT_TRUE(SUCCEEDED(stg->Stat(&st, STATFLAG_NONAME)));
    EXPECT_EQ(st.clsid, CLSID_NULL);
}

TEST_P(StressCreateConformance, RootStateBitsZero) {
    auto path = temp_file("sc_rbits");
    guard_.add(path);
    {
        auto cf = compound_file::create(path, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
    }
    auto cf = compound_file::open(path, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    EXPECT_EQ(cf->root_storage().state_bits(), 0u);
}

TEST_P(StressCreateConformance, RootChildrenEmpty) {
    auto path = temp_file("sc_rchildren");
    guard_.add(path);
    {
        auto cf = compound_file::create(path, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
    }
    auto cf = compound_file::open(path, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    EXPECT_EQ(cf->root_storage().children().size(), 0u);
}

TEST_P(StressCreateConformance, RootNameIsRootEntry) {
    auto path = temp_file("sc_rname");
    guard_.add(path);
    {
        auto cf = compound_file::create(path, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
    }
    auto cf = compound_file::open(path, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    EXPECT_EQ(cf->root_storage().name(), "Root Entry");
}

// ── Overwrite / multiple creates ────────────────────────────────────────

TEST_P(StressCreateConformance, CreateOverwritesExisting) {
    auto path = temp_file("sc_overwrite");
    guard_.add(path);
    {
        auto cf = compound_file::create(path, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("Old");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    {
        auto cf = compound_file::create(path, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(path, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    EXPECT_EQ(cf->root_storage().children().size(), 0u);
}

TEST_P(StressCreateConformance, MultipleCreatesInSequence) {
    for (int i = 0; i < 5; ++i) {
        auto path = temp_file("sc_multi" + std::to_string(i));
        guard_.add(path);
        auto cf = compound_file::create(path, GetParam().ver);
        ASSERT_TRUE(cf.has_value()) << "Failed at iteration " << i;
        auto s = cf->root_storage().create_stream("S");
        ASSERT_TRUE(s.has_value());
        auto data = make_test_data(100, static_cast<uint8_t>(i));
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
}

// ── In-memory ───────────────────────────────────────────────────────────

TEST_P(StressCreateConformance, InMemoryCreate) {
    auto cf = compound_file::create_in_memory(GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    EXPECT_EQ(cf->version(), GetParam().ver);
    EXPECT_EQ(cf->root_storage().children().size(), 0u);
}

TEST_P(StressCreateConformance, InMemoryDataNotNull) {
    auto cf = compound_file::create_in_memory(GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    cf->flush();
    auto *d = cf->data();
    ASSERT_NE(d, nullptr);
    EXPECT_GE(d->size(), static_cast<size_t>(GetParam().sector_size));
}

TEST_P(StressCreateConformance, InMemorySerializeWin32Opens) {
    auto cf = compound_file::create_in_memory(GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    auto s = cf->root_storage().create_stream("Test");
    ASSERT_TRUE(s.has_value());
    auto data = make_test_data(200);
    ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
    ASSERT_TRUE(cf->flush().has_value());
    auto *raw = cf->data();
    ASSERT_NE(raw, nullptr);

    // Write to temp file and open with Win32
    auto path = temp_file("sc_inmem");
    guard_.add(path);
    {
        std::ofstream ofs(path, std::ios::binary);
        ofs.write(reinterpret_cast<const char *>(raw->data()), raw->size());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"Test", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 200u);
}

TEST_P(StressCreateConformance, OpenFromMemoryRoundtrip) {
    // Create in-memory, serialize, open from memory
    auto cf1 = compound_file::create_in_memory(GetParam().ver);
    ASSERT_TRUE(cf1.has_value());
    auto s = cf1->root_storage().create_stream("RT");
    ASSERT_TRUE(s.has_value());
    auto data = make_test_data(150, 0xAB);
    ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
    ASSERT_TRUE(cf1->flush().has_value());
    auto *raw = cf1->data();
    ASSERT_NE(raw, nullptr);

    auto cf2 = compound_file::open_from_memory(*raw);
    ASSERT_TRUE(cf2.has_value());
    auto s2 = cf2->root_storage().open_stream("RT");
    ASSERT_TRUE(s2.has_value());
    EXPECT_EQ(s2->size(), 150u);
    std::vector<uint8_t> buf(150);
    ASSERT_TRUE(s2->read(0, std::span<uint8_t>(buf)).has_value());
    EXPECT_EQ(buf, data);
}

// ── Create + close without flush ────────────────────────────────────────

TEST_P(StressCreateConformance, CreateNoExplicitFlush) {
    auto path = temp_file("sc_noflush");
    guard_.add(path);
    {
        auto cf = compound_file::create(path, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
    }
    // File should still be valid
    auto sz = std::filesystem::file_size(path);
    EXPECT_GT(sz, 0u);
}

// ── Version query ───────────────────────────────────────────────────────

TEST_P(StressCreateConformance, VersionQueryAfterCreate) {
    auto cf = compound_file::create_in_memory(GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    EXPECT_EQ(cf->version(), GetParam().ver);
}

TEST_P(StressCreateConformance, VersionQueryAfterOpen) {
    auto path = temp_file("sc_veropen");
    guard_.add(path);
    {
        auto cf = compound_file::create(path, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
    }
    auto cf = compound_file::open(path, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    EXPECT_EQ(cf->version(), GetParam().ver);
}

// ── Create with stream, Win32 verifies ──────────────────────────────────

TEST_P(StressCreateConformance, CreateWithStreamWin32Reads) {
    auto path = temp_file("sc_withstrm");
    guard_.add(path);
    auto data = make_test_data(300, 0x42);
    {
        auto cf = compound_file::create(path, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto s = cf->root_storage().create_stream("Data");
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"Data", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 300u);
    std::vector<uint8_t> buf(300);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 300, &rc)));
    EXPECT_EQ(buf, data);
}

TEST_P(StressCreateConformance, CreateWithStorageWin32Opens) {
    auto path = temp_file("sc_withstg");
    guard_.add(path);
    {
        auto cf = compound_file::create(path, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->root_storage().create_storage("Sub").has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(path.wstring(), stg.put())));
    storage_ptr sub;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStorage(L"Sub", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0, sub.put())));
}

// ── Win32 creates with content, Stout reads ─────────────────────────────

TEST_P(StressCreateConformance, Win32CreateWithStreamStoutReads) {
    auto path = temp_file("sc_w32strm");
    guard_.add(path);
    auto data = make_test_data(400, 0x77);
    {
        storage_ptr stg;
        if (GetParam().ver == cfb_version::v4) {
            ASSERT_TRUE(SUCCEEDED(win32_create_v4(path.wstring(), stg.put())));
        } else {
            ASSERT_TRUE(SUCCEEDED(win32_create_v3(path.wstring(), stg.put())));
        }
        stream_ptr strm;
        ASSERT_TRUE(SUCCEEDED(
            stg->CreateStream(L"W32Data", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, strm.put())));
        ASSERT_TRUE(SUCCEEDED(win32_stream_write(strm.get(), data.data(), 400)));
    }
    auto cf = compound_file::open(path, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto s = cf->root_storage().open_stream("W32Data");
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->size(), 400u);
    std::vector<uint8_t> buf(400);
    ASSERT_TRUE(s->read(0, std::span<uint8_t>(buf)).has_value());
    EXPECT_EQ(buf, data);
}

TEST_P(StressCreateConformance, Win32CreateWithStorageStoutOpens) {
    auto path = temp_file("sc_w32stg");
    guard_.add(path);
    {
        storage_ptr stg;
        if (GetParam().ver == cfb_version::v4) {
            ASSERT_TRUE(SUCCEEDED(win32_create_v4(path.wstring(), stg.put())));
        } else {
            ASSERT_TRUE(SUCCEEDED(win32_create_v3(path.wstring(), stg.put())));
        }
        storage_ptr sub;
        ASSERT_TRUE(SUCCEEDED(
            stg->CreateStorage(L"W32Sub", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, sub.put())));
    }
    auto cf = compound_file::open(path, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto sub = cf->root_storage().open_storage("W32Sub");
    ASSERT_TRUE(sub.has_value());
}

// ── Flush idempotent ────────────────────────────────────────────────────

TEST_P(StressCreateConformance, DoubleFlush) {
    auto path = temp_file("sc_dflush");
    guard_.add(path);
    auto cf = compound_file::create(path, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    ASSERT_TRUE(cf->flush().has_value());
    ASSERT_TRUE(cf->flush().has_value());
    auto sz = std::filesystem::file_size(path);
    EXPECT_GT(sz, 0u);
}

TEST_P(StressCreateConformance, FlushAfterWritePreservesData) {
    auto path = temp_file("sc_flushwr");
    guard_.add(path);
    auto data = make_test_data(500, 0x33);
    auto cf = compound_file::create(path, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    auto s = cf->root_storage().create_stream("D");
    ASSERT_TRUE(s.has_value());
    ASSERT_TRUE(s->write(0, std::span<const uint8_t>(data)).has_value());
    ASSERT_TRUE(cf->flush().has_value());
    // Write more, flush again
    auto data2 = make_test_data(200, 0x44);
    auto s2 = cf->root_storage().create_stream("D2");
    ASSERT_TRUE(s2.has_value());
    ASSERT_TRUE(s2->write(0, std::span<const uint8_t>(data2)).has_value());
    ASSERT_TRUE(cf->flush().has_value());
    // Verify both exist — close cf first
    { auto tmp = std::move(cf); }
    auto cf2 = compound_file::open(path, open_mode::read);
    ASSERT_TRUE(cf2.has_value());
    EXPECT_EQ(cf2->root_storage().children().size(), 2u);
}

#endif // _WIN32
