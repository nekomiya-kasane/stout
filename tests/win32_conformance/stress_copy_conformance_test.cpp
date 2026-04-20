#ifdef _WIN32

#include "conformance_utils.h"

#include <gtest/gtest.h>
#include <stout/compound_file.h>

using namespace conformance;
using namespace stout;

struct VPCopy {
    cfb_version ver;
    uint16_t major;
};

class StressCopyConformance : public ::testing::TestWithParam<VPCopy> {
  protected:
    com_init com_;
    temp_file_guard guard_;
};

static const VPCopy vp_copy[] = {{cfb_version::v3, 3}, {cfb_version::v4, 4}};

INSTANTIATE_TEST_SUITE_P(V, StressCopyConformance, ::testing::ValuesIn(vp_copy),
                         [](const auto &info) { return info.param.major == 3 ? "V3" : "V4"; });

// ── stream::copy_to basic ───────────────────────────────────────────────

TEST_P(StressCopyConformance, StreamCopyToBasic) {
    auto p = temp_file("sc_basic");
    guard_.add(p);
    auto data = make_test_data(500, 0x11);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto src = cf->root_storage().create_stream("Src");
        ASSERT_TRUE(src.has_value());
        ASSERT_TRUE(src->write(0, std::span<const uint8_t>(data)).has_value());
        auto dst = cf->root_storage().create_stream("Dst");
        ASSERT_TRUE(dst.has_value());
        auto copied = src->copy_to(*dst, 500);
        ASSERT_TRUE(copied.has_value());
        EXPECT_EQ(*copied, 500u);
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto dst = cf->root_storage().open_stream("Dst");
    ASSERT_TRUE(dst.has_value());
    EXPECT_EQ(dst->size(), 500u);
    std::vector<uint8_t> buf(500);
    ASSERT_TRUE(dst->read(0, std::span<uint8_t>(buf)).has_value());
    EXPECT_EQ(buf, data);
}

TEST_P(StressCopyConformance, StreamCopyToPartial) {
    auto p = temp_file("sc_partial");
    guard_.add(p);
    auto data = make_test_data(1000, 0x22);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto src = cf->root_storage().create_stream("Src");
        ASSERT_TRUE(src.has_value());
        ASSERT_TRUE(src->write(0, std::span<const uint8_t>(data)).has_value());
        auto dst = cf->root_storage().create_stream("Dst");
        ASSERT_TRUE(dst.has_value());
        auto copied = src->copy_to(*dst, 300);
        ASSERT_TRUE(copied.has_value());
        EXPECT_EQ(*copied, 300u);
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto dst = cf->root_storage().open_stream("Dst");
    ASSERT_TRUE(dst.has_value());
    EXPECT_GE(dst->size(), 300u);
    std::vector<uint8_t> buf(300);
    ASSERT_TRUE(dst->read(0, std::span<uint8_t>(buf)).has_value());
    auto expected = std::vector<uint8_t>(data.begin(), data.begin() + 300);
    EXPECT_EQ(buf, expected);
}

TEST_P(StressCopyConformance, StreamCopyToZeroBytes) {
    auto p = temp_file("sc_zero");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto src = cf->root_storage().create_stream("Src");
        ASSERT_TRUE(src.has_value());
        auto dst = cf->root_storage().create_stream("Dst");
        ASSERT_TRUE(dst.has_value());
        auto copied = src->copy_to(*dst, 0);
        ASSERT_TRUE(copied.has_value());
        EXPECT_EQ(*copied, 0u);
        ASSERT_TRUE(cf->flush().has_value());
    }
}

TEST_P(StressCopyConformance, StreamCopyToMoreThanAvailable) {
    auto p = temp_file("sc_more");
    guard_.add(p);
    auto data = make_test_data(200, 0x33);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto src = cf->root_storage().create_stream("Src");
        ASSERT_TRUE(src.has_value());
        ASSERT_TRUE(src->write(0, std::span<const uint8_t>(data)).has_value());
        auto dst = cf->root_storage().create_stream("Dst");
        ASSERT_TRUE(dst.has_value());
        auto copied = src->copy_to(*dst, 10000);
        ASSERT_TRUE(copied.has_value());
        EXPECT_EQ(*copied, 200u);
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto dst = cf->root_storage().open_stream("Dst");
    ASSERT_TRUE(dst.has_value());
    EXPECT_GE(dst->size(), 200u);
}

// ── stream::copy_to cross-validated with Win32 ──────────────────────────

TEST_P(StressCopyConformance, StreamCopyToWin32Reads) {
    auto p = temp_file("sc_w32");
    guard_.add(p);
    auto data = make_test_data(800, 0x44);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto src = cf->root_storage().create_stream("Src");
        ASSERT_TRUE(src.has_value());
        ASSERT_TRUE(src->write(0, std::span<const uint8_t>(data)).has_value());
        auto dst = cf->root_storage().create_stream("Dst");
        ASSERT_TRUE(dst.has_value());
        ASSERT_TRUE(src->copy_to(*dst, 800).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    storage_ptr stg;
    ASSERT_TRUE(SUCCEEDED(win32_open_read(p.wstring(), stg.put())));
    stream_ptr strm;
    ASSERT_TRUE(SUCCEEDED(stg->OpenStream(L"Dst", nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, strm.put())));
    EXPECT_EQ(win32_stream_size(strm.get()), 800u);
    std::vector<uint8_t> buf(800);
    ULONG rc = 0;
    ASSERT_TRUE(SUCCEEDED(win32_stream_read(strm.get(), buf.data(), 800, &rc)));
    EXPECT_EQ(buf, data);
}

// ── stream::copy_to mini stream ─────────────────────────────────────────

TEST_P(StressCopyConformance, StreamCopyToMiniStream) {
    auto p = temp_file("sc_mini");
    guard_.add(p);
    auto data = make_test_data(100, 0x55);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto src = cf->root_storage().create_stream("Src");
        ASSERT_TRUE(src.has_value());
        ASSERT_TRUE(src->write(0, std::span<const uint8_t>(data)).has_value());
        auto dst = cf->root_storage().create_stream("Dst");
        ASSERT_TRUE(dst.has_value());
        ASSERT_TRUE(src->copy_to(*dst, 100).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto dst = cf->root_storage().open_stream("Dst");
    ASSERT_TRUE(dst.has_value());
    EXPECT_EQ(dst->size(), 100u);
    std::vector<uint8_t> buf(100);
    ASSERT_TRUE(dst->read(0, std::span<uint8_t>(buf)).has_value());
    EXPECT_EQ(buf, data);
}

// ── stream::copy_to regular stream ──────────────────────────────────────

TEST_P(StressCopyConformance, StreamCopyToRegularStream) {
    auto p = temp_file("sc_reg");
    guard_.add(p);
    auto data = make_test_data(5000, 0x66);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto src = cf->root_storage().create_stream("Src");
        ASSERT_TRUE(src.has_value());
        ASSERT_TRUE(src->write(0, std::span<const uint8_t>(data)).has_value());
        auto dst = cf->root_storage().create_stream("Dst");
        ASSERT_TRUE(dst.has_value());
        ASSERT_TRUE(src->copy_to(*dst, 5000).has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto dst = cf->root_storage().open_stream("Dst");
    ASSERT_TRUE(dst.has_value());
    EXPECT_EQ(dst->size(), 5000u);
    std::vector<uint8_t> buf(5000);
    ASSERT_TRUE(dst->read(0, std::span<uint8_t>(buf)).has_value());
    EXPECT_EQ(buf, data);
}

// ── storage::copy_to ────────────────────────────────────────────────────

TEST_P(StressCopyConformance, StorageCopyToStream) {
    auto p = temp_file("sc_stgstrm");
    guard_.add(p);
    auto data = make_test_data(300, 0x77);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto root = cf->root_storage();
        auto src = root.create_stream("Src");
        ASSERT_TRUE(src.has_value());
        ASSERT_TRUE(src->write(0, std::span<const uint8_t>(data)).has_value());
        auto dest_stg = root.create_storage("Dest");
        ASSERT_TRUE(dest_stg.has_value());
        auto r = root.copy_to(*dest_stg, "Src");
        ASSERT_TRUE(r.has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto dest = cf->root_storage().open_storage("Dest");
    ASSERT_TRUE(dest.has_value());
    auto s = dest->open_stream("Src");
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->size(), 300u);
    std::vector<uint8_t> buf(300);
    ASSERT_TRUE(s->read(0, std::span<uint8_t>(buf)).has_value());
    EXPECT_EQ(buf, data);
}

TEST_P(StressCopyConformance, StorageCopyToNotFound) {
    auto p = temp_file("sc_stgnf");
    guard_.add(p);
    auto cf = compound_file::create(p, GetParam().ver);
    ASSERT_TRUE(cf.has_value());
    auto dest = cf->root_storage().create_storage("Dest");
    ASSERT_TRUE(dest.has_value());
    auto r = cf->root_storage().copy_to(*dest, "Ghost");
    EXPECT_FALSE(r.has_value());
}

// ── Copy preserves data integrity ───────────────────────────────────────

TEST_P(StressCopyConformance, CopyDoesNotAffectSource) {
    auto p = temp_file("sc_srcok");
    guard_.add(p);
    auto data = make_test_data(400, 0x88);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        auto src = cf->root_storage().create_stream("Src");
        ASSERT_TRUE(src.has_value());
        ASSERT_TRUE(src->write(0, std::span<const uint8_t>(data)).has_value());
        auto dst = cf->root_storage().create_stream("Dst");
        ASSERT_TRUE(dst.has_value());
        ASSERT_TRUE(src->copy_to(*dst, 400).has_value());
        // Verify source still intact
        std::vector<uint8_t> buf(400);
        ASSERT_TRUE(src->read(0, std::span<uint8_t>(buf)).has_value());
        EXPECT_EQ(buf, data);
        ASSERT_TRUE(cf->flush().has_value());
    }
}

#endif // _WIN32
