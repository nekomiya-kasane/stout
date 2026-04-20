#include <gtest/gtest.h>
#include "stout/ole/property_set.h"

using namespace stout;
using namespace stout::ole;

// ── property_section basic operations ──────────────────────────────────

TEST(PropertySectionTest, SetAndGet) {
    property_section sec;
    sec.set_string(pid::title, "My Document");
    sec.set_i4(pid::page_count, 42);

    EXPECT_EQ(sec.get_string(pid::title), "My Document");
    EXPECT_EQ(sec.get_i4(pid::page_count), 42);
}

TEST(PropertySectionTest, GetMissing) {
    property_section sec;
    EXPECT_EQ(sec.get_string(pid::title), "");
    EXPECT_EQ(sec.get_i4(pid::page_count), 0);
    EXPECT_EQ(sec.get(pid::title), nullptr);
}

TEST(PropertySectionTest, Remove) {
    property_section sec;
    sec.set_string(pid::title, "Test");
    EXPECT_NE(sec.get(pid::title), nullptr);
    sec.remove(pid::title);
    EXPECT_EQ(sec.get(pid::title), nullptr);
}

TEST(PropertySectionTest, SetU4) {
    property_section sec;
    sec.set_u4(pid::security, 0x12345678);
    EXPECT_EQ(sec.get_u4(pid::security), 0x12345678u);
}

TEST(PropertySectionTest, SetFiletime) {
    property_section sec;
    sec.set_filetime(pid::create_dtm, 0xAABBCCDDEEFF0011ULL);
    EXPECT_EQ(sec.get_filetime(pid::create_dtm), 0xAABBCCDDEEFF0011ULL);
}

TEST(PropertySectionTest, SetBool) {
    property_section sec;
    sec.set_bool(0x100, true);
    EXPECT_TRUE(sec.get_bool(0x100));
    sec.set_bool(0x100, false);
    EXPECT_FALSE(sec.get_bool(0x100));
}

// ── property_set operations ────────────────────────────────────────────

TEST(PropertySetTest, AddSection) {
    property_set ps;
    auto& sec = ps.add_section(fmtid_summary_information());
    sec.set_string(pid::title, "Hello");

    auto* found = ps.section(fmtid_summary_information());
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->get_string(pid::title), "Hello");
}

TEST(PropertySetTest, SectionNotFound) {
    property_set ps;
    EXPECT_EQ(ps.section(fmtid_summary_information()), nullptr);
}

TEST(PropertySetTest, AddSectionIdempotent) {
    property_set ps;
    auto& s1 = ps.add_section(fmtid_summary_information());
    s1.set_string(pid::title, "First");
    auto& s2 = ps.add_section(fmtid_summary_information());
    EXPECT_EQ(s2.get_string(pid::title), "First");
    EXPECT_EQ(ps.sections.size(), 1u);
}

// ── Roundtrip: serialize then parse ────────────────────────────────────

TEST(PropertySetRoundtrip, EmptySection) {
    property_set ps;
    ps.add_section(fmtid_summary_information());

    auto data = serialize_property_set(ps);
    ASSERT_TRUE(data.has_value());
    ASSERT_GT(data->size(), 28u);

    auto ps2 = parse_property_set(*data);
    ASSERT_TRUE(ps2.has_value());
    EXPECT_EQ(ps2->sections.size(), 1u);
    EXPECT_EQ(ps2->sections[0].fmtid, fmtid_summary_information());
}

TEST(PropertySetRoundtrip, StringProperty) {
    property_set ps;
    auto& sec = ps.add_section(fmtid_summary_information());
    sec.set_string(pid::title, "Test Title");
    sec.set_string(pid::author, "John Doe");

    auto data = serialize_property_set(ps);
    ASSERT_TRUE(data.has_value());

    auto ps2 = parse_property_set(*data);
    ASSERT_TRUE(ps2.has_value());
    ASSERT_EQ(ps2->sections.size(), 1u);
    EXPECT_EQ(ps2->sections[0].get_string(pid::title), "Test Title");
    EXPECT_EQ(ps2->sections[0].get_string(pid::author), "John Doe");
}

TEST(PropertySetRoundtrip, IntegerProperties) {
    property_set ps;
    auto& sec = ps.add_section(fmtid_summary_information());
    sec.set_i4(pid::page_count, 100);
    sec.set_u4(pid::security, 0xDEADBEEF);

    auto data = serialize_property_set(ps);
    ASSERT_TRUE(data.has_value());

    auto ps2 = parse_property_set(*data);
    ASSERT_TRUE(ps2.has_value());
    ASSERT_EQ(ps2->sections.size(), 1u);
    EXPECT_EQ(ps2->sections[0].get_i4(pid::page_count), 100);
    EXPECT_EQ(ps2->sections[0].get_u4(pid::security), 0xDEADBEEFu);
}

TEST(PropertySetRoundtrip, FiletimeProperty) {
    property_set ps;
    auto& sec = ps.add_section(fmtid_summary_information());
    sec.set_filetime(pid::create_dtm, 0x0102030405060708ULL);

    auto data = serialize_property_set(ps);
    ASSERT_TRUE(data.has_value());

    auto ps2 = parse_property_set(*data);
    ASSERT_TRUE(ps2.has_value());
    EXPECT_EQ(ps2->sections[0].get_filetime(pid::create_dtm), 0x0102030405060708ULL);
}

TEST(PropertySetRoundtrip, BoolProperty) {
    property_set ps;
    auto& sec = ps.add_section(fmtid_summary_information());
    sec.set_bool(0x100, true);

    auto data = serialize_property_set(ps);
    ASSERT_TRUE(data.has_value());

    auto ps2 = parse_property_set(*data);
    ASSERT_TRUE(ps2.has_value());
    EXPECT_TRUE(ps2->sections[0].get_bool(0x100));
}

TEST(PropertySetRoundtrip, FloatProperty) {
    property_set ps;
    auto& sec = ps.add_section(fmtid_summary_information());
    sec.set(0x200, vt::r4, 3.14f);

    auto data = serialize_property_set(ps);
    ASSERT_TRUE(data.has_value());

    auto ps2 = parse_property_set(*data);
    ASSERT_TRUE(ps2.has_value());
    auto* p = ps2->sections[0].get(0x200);
    ASSERT_NE(p, nullptr);
    auto* f = std::get_if<float>(&p->value);
    ASSERT_NE(f, nullptr);
    EXPECT_NEAR(*f, 3.14f, 0.001f);
}

TEST(PropertySetRoundtrip, DoubleProperty) {
    property_set ps;
    auto& sec = ps.add_section(fmtid_summary_information());
    sec.set(0x201, vt::r8, 2.71828);

    auto data = serialize_property_set(ps);
    ASSERT_TRUE(data.has_value());

    auto ps2 = parse_property_set(*data);
    ASSERT_TRUE(ps2.has_value());
    auto* p = ps2->sections[0].get(0x201);
    ASSERT_NE(p, nullptr);
    auto* d = std::get_if<double>(&p->value);
    ASSERT_NE(d, nullptr);
    EXPECT_NEAR(*d, 2.71828, 0.00001);
}

TEST(PropertySetRoundtrip, BlobProperty) {
    property_set ps;
    auto& sec = ps.add_section(fmtid_summary_information());
    std::vector<uint8_t> blob = {0x01, 0x02, 0x03, 0x04, 0x05};
    sec.set(0x300, vt::blob, blob);

    auto data = serialize_property_set(ps);
    ASSERT_TRUE(data.has_value());

    auto ps2 = parse_property_set(*data);
    ASSERT_TRUE(ps2.has_value());
    auto* p = ps2->sections[0].get(0x300);
    ASSERT_NE(p, nullptr);
    auto* b = std::get_if<std::vector<uint8_t>>(&p->value);
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(*b, blob);
}

TEST(PropertySetRoundtrip, GuidProperty) {
    property_set ps;
    auto& sec = ps.add_section(fmtid_summary_information());
    guid g = {0x12345678, 0xABCD, 0xEF01, {0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0x01}};
    sec.set(0x400, vt::clsid, g);

    auto data = serialize_property_set(ps);
    ASSERT_TRUE(data.has_value());

    auto ps2 = parse_property_set(*data);
    ASSERT_TRUE(ps2.has_value());
    auto* p = ps2->sections[0].get(0x400);
    ASSERT_NE(p, nullptr);
    auto* gp = std::get_if<guid>(&p->value);
    ASSERT_NE(gp, nullptr);
    EXPECT_EQ(*gp, g);
}

TEST(PropertySetRoundtrip, TwoSections) {
    property_set ps;
    auto& s1 = ps.add_section(fmtid_summary_information());
    s1.set_string(pid::title, "Title");

    auto& s2 = ps.add_section(fmtid_doc_summary_information());
    s2.set_i4(0x02, 99);

    auto data = serialize_property_set(ps);
    ASSERT_TRUE(data.has_value());

    auto ps2 = parse_property_set(*data);
    ASSERT_TRUE(ps2.has_value());
    ASSERT_EQ(ps2->sections.size(), 2u);

    auto* sec1 = ps2->section(fmtid_summary_information());
    ASSERT_NE(sec1, nullptr);
    EXPECT_EQ(sec1->get_string(pid::title), "Title");

    auto* sec2 = ps2->section(fmtid_doc_summary_information());
    ASSERT_NE(sec2, nullptr);
    EXPECT_EQ(sec2->get_i4(0x02), 99);
}

TEST(PropertySetRoundtrip, MixedTypes) {
    property_set ps;
    auto& sec = ps.add_section(fmtid_summary_information());
    sec.set_string(pid::title, "Mixed");
    sec.set_i4(pid::page_count, 7);
    sec.set_filetime(pid::create_dtm, 132000000000000000ULL);
    sec.set_bool(0x500, false);
    sec.set_u4(pid::security, 1);

    auto data = serialize_property_set(ps);
    ASSERT_TRUE(data.has_value());

    auto ps2 = parse_property_set(*data);
    ASSERT_TRUE(ps2.has_value());
    auto& s = ps2->sections[0];
    EXPECT_EQ(s.get_string(pid::title), "Mixed");
    EXPECT_EQ(s.get_i4(pid::page_count), 7);
    EXPECT_EQ(s.get_filetime(pid::create_dtm), 132000000000000000ULL);
    EXPECT_FALSE(s.get_bool(0x500));
    EXPECT_EQ(s.get_u4(pid::security), 1u);
}

// ── Well-known FMTIDs ──────────────────────────────────────────────────

TEST(PropertySetTest, FmtidConstants) {
    EXPECT_FALSE(fmtid_summary_information().is_null());
    EXPECT_FALSE(fmtid_doc_summary_information().is_null());
    EXPECT_NE(fmtid_summary_information(), fmtid_doc_summary_information());
}

// ── Parse error handling ───────────────────────────────────────────────

TEST(PropertySetTest, ParseTooSmall) {
    std::vector<uint8_t> data(10, 0);
    auto result = parse_property_set(data);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), error::corrupt_file);
}
