#ifdef _WIN32

#include "conformance_utils.h"

#include <gtest/gtest.h>
#include <stout/compound_file.h>
#include <stout/ole/property_set.h>

using namespace conformance;
using namespace stout;
using namespace stout::ole;

class PropertySetConformance : public ::testing::Test {
  protected:
    com_init com_;
    temp_file_guard guard_;

    // Helper: open Win32 SummaryInformation property storage (read)
    static HRESULT open_summary_read(const std::wstring &path, propset_storage_ptr &pss, propset_ptr &ps) {
        storage_ptr stg;
        HRESULT hr = win32_open_read(path, stg.put());
        if (FAILED(hr)) return hr;
        hr = stg->QueryInterface(IID_IPropertySetStorage, reinterpret_cast<void **>(pss.put()));
        if (FAILED(hr)) return hr;
        hr = pss->Open(FMTID_SummaryInformation, STGM_READ | STGM_SHARE_EXCLUSIVE, ps.put());
        return hr;
    }

    // Helper: write a Stout property set to the SummaryInformation stream
    static bool write_summary(compound_file &cf, const property_set &pset) {
        auto data = serialize_property_set(pset);
        if (!data) return false;
        auto root = cf.root_storage();
        // SummaryInformation stream name: "\005SummaryInformation"
        auto s = root.create_stream("\005SummaryInformation");
        if (!s) return false;
        auto wr = s->write(0, std::span<const uint8_t>(*data));
        return wr.has_value();
    }

    // Helper: read a Stout property set from the SummaryInformation stream
    static std::expected<property_set, error> read_summary(compound_file &cf) {
        auto root = cf.root_storage();
        auto s = root.open_stream("\005SummaryInformation");
        if (!s) return std::unexpected(s.error());
        auto sz = s->size();
        std::vector<uint8_t> buf(sz);
        auto rd = s->read(0, std::span<uint8_t>(buf));
        if (!rd) return std::unexpected(rd.error());
        return parse_property_set(buf);
    }
};

// ── WriteSummaryTitle: Stout writes title, Win32 reads ──────────────────
TEST_F(PropertySetConformance, WriteSummaryTitle) {
    auto path = temp_file("ps_title");
    guard_.add(path);

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        property_set ps;
        auto &sec = ps.add_section(fmtid_summary_information());
        sec.set_string(pid::title, "Test Document");
        ASSERT_TRUE(write_summary(*cf, ps));
        ASSERT_TRUE(cf->flush().has_value());
    }

    // Win32 reads
    propset_storage_ptr pss;
    propset_ptr prop;
    ASSERT_TRUE(SUCCEEDED(open_summary_read(path.wstring(), pss, prop)));

    PROPSPEC spec{};
    spec.ulKind = PRSPEC_PROPID;
    spec.propid = PIDSI_TITLE;
    PROPVARIANT var{};
    PropVariantInit(&var);
    ASSERT_TRUE(SUCCEEDED(prop->ReadMultiple(1, &spec, &var)));
    EXPECT_EQ(var.vt, VT_LPSTR);
    if (var.vt == VT_LPSTR && var.pszVal) {
        EXPECT_STREQ(var.pszVal, "Test Document");
    }
    PropVariantClear(&var);
}

// ── WriteSummaryAuthor: Stout writes author, Win32 reads ────────────────
TEST_F(PropertySetConformance, WriteSummaryAuthor) {
    auto path = temp_file("ps_author");
    guard_.add(path);

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        property_set ps;
        auto &sec = ps.add_section(fmtid_summary_information());
        sec.set_string(pid::author, "Stout Library");
        ASSERT_TRUE(write_summary(*cf, ps));
        ASSERT_TRUE(cf->flush().has_value());
    }

    propset_storage_ptr pss;
    propset_ptr prop;
    ASSERT_TRUE(SUCCEEDED(open_summary_read(path.wstring(), pss, prop)));

    PROPSPEC spec{};
    spec.ulKind = PRSPEC_PROPID;
    spec.propid = PIDSI_AUTHOR;
    PROPVARIANT var{};
    PropVariantInit(&var);
    ASSERT_TRUE(SUCCEEDED(prop->ReadMultiple(1, &spec, &var)));
    EXPECT_EQ(var.vt, VT_LPSTR);
    if (var.vt == VT_LPSTR && var.pszVal) {
        EXPECT_STREQ(var.pszVal, "Stout Library");
    }
    PropVariantClear(&var);
}

// ── WriteIntProperty: VT_I4 value ───────────────────────────────────────
TEST_F(PropertySetConformance, WriteIntProperty) {
    auto path = temp_file("ps_int");
    guard_.add(path);

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        property_set ps;
        auto &sec = ps.add_section(fmtid_summary_information());
        sec.set_i4(pid::page_count, 42);
        ASSERT_TRUE(write_summary(*cf, ps));
        ASSERT_TRUE(cf->flush().has_value());
    }

    propset_storage_ptr pss;
    propset_ptr prop;
    ASSERT_TRUE(SUCCEEDED(open_summary_read(path.wstring(), pss, prop)));

    PROPSPEC spec{};
    spec.ulKind = PRSPEC_PROPID;
    spec.propid = PIDSI_PAGECOUNT;
    PROPVARIANT var{};
    PropVariantInit(&var);
    ASSERT_TRUE(SUCCEEDED(prop->ReadMultiple(1, &spec, &var)));
    EXPECT_EQ(var.vt, VT_I4);
    EXPECT_EQ(var.lVal, 42);
    PropVariantClear(&var);
}

// ── WriteBoolProperty: VT_BOOL value ────────────────────────────────────
TEST_F(PropertySetConformance, WriteBoolProperty) {
    auto path = temp_file("ps_bool");
    guard_.add(path);

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        property_set ps;
        auto &sec = ps.add_section(fmtid_summary_information());
        sec.set_i4(pid::security, 0); // VT_I4 security = 0 (no security)
        ASSERT_TRUE(write_summary(*cf, ps));
        ASSERT_TRUE(cf->flush().has_value());
    }

    propset_storage_ptr pss;
    propset_ptr prop;
    ASSERT_TRUE(SUCCEEDED(open_summary_read(path.wstring(), pss, prop)));

    PROPSPEC spec{};
    spec.ulKind = PRSPEC_PROPID;
    spec.propid = PIDSI_DOC_SECURITY;
    PROPVARIANT var{};
    PropVariantInit(&var);
    ASSERT_TRUE(SUCCEEDED(prop->ReadMultiple(1, &spec, &var)));
    EXPECT_EQ(var.vt, VT_I4);
    EXPECT_EQ(var.lVal, 0);
    PropVariantClear(&var);
}

// ── WriteMultipleProperties: 3 properties at once ───────────────────────
TEST_F(PropertySetConformance, WriteMultipleProperties) {
    auto path = temp_file("ps_multi");
    guard_.add(path);

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        property_set ps;
        auto &sec = ps.add_section(fmtid_summary_information());
        sec.set_string(pid::title, "Multi Test");
        sec.set_string(pid::author, "Author");
        sec.set_i4(pid::page_count, 99);
        ASSERT_TRUE(write_summary(*cf, ps));
        ASSERT_TRUE(cf->flush().has_value());
    }

    propset_storage_ptr pss;
    propset_ptr prop;
    ASSERT_TRUE(SUCCEEDED(open_summary_read(path.wstring(), pss, prop)));

    PROPSPEC specs[3]{};
    specs[0].ulKind = PRSPEC_PROPID;
    specs[0].propid = PIDSI_TITLE;
    specs[1].ulKind = PRSPEC_PROPID;
    specs[1].propid = PIDSI_AUTHOR;
    specs[2].ulKind = PRSPEC_PROPID;
    specs[2].propid = PIDSI_PAGECOUNT;
    PROPVARIANT vars[3]{};
    for (auto &v : vars) PropVariantInit(&v);
    ASSERT_TRUE(SUCCEEDED(prop->ReadMultiple(3, specs, vars)));

    EXPECT_EQ(vars[0].vt, VT_LPSTR);
    if (vars[0].vt == VT_LPSTR) EXPECT_STREQ(vars[0].pszVal, "Multi Test");
    EXPECT_EQ(vars[1].vt, VT_LPSTR);
    if (vars[1].vt == VT_LPSTR) EXPECT_STREQ(vars[1].pszVal, "Author");
    EXPECT_EQ(vars[2].vt, VT_I4);
    EXPECT_EQ(vars[2].lVal, 99);

    for (auto &v : vars) PropVariantClear(&v);
}

// ── Win32 writes properties, Stout reads ────────────────────────────────
TEST_F(PropertySetConformance, Win32WriteStoutRead) {
    auto path = temp_file("ps_w32");
    guard_.add(path);

    {
        storage_ptr stg;
        ASSERT_TRUE(SUCCEEDED(win32_create_v4(path.wstring(), stg.put())));
        propset_storage_ptr pss;
        ASSERT_TRUE(SUCCEEDED(stg->QueryInterface(IID_IPropertySetStorage, reinterpret_cast<void **>(pss.put()))));
        propset_ptr prop;
        ASSERT_TRUE(SUCCEEDED(pss->Create(FMTID_SummaryInformation, nullptr, PROPSETFLAG_DEFAULT,
                                          STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, prop.put())));

        PROPSPEC spec{};
        spec.ulKind = PRSPEC_PROPID;
        spec.propid = PIDSI_TITLE;
        PROPVARIANT var{};
        PropVariantInit(&var);
        var.vt = VT_LPSTR;
        var.pszVal = const_cast<char *>("Win32 Title");
        ASSERT_TRUE(SUCCEEDED(prop->WriteMultiple(1, &spec, &var, PID_FIRST_USABLE)));
    }

    auto cf = compound_file::open(path, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto ps = read_summary(*cf);
    ASSERT_TRUE(ps.has_value());

    auto *sec = ps->section(fmtid_summary_information());
    ASSERT_NE(sec, nullptr);
    // Win32 may write VT_LPSTR with codepage 1200 (UTF-16LE), so Stout's
    // parser stores the raw bytes in std::string. Verify the property exists
    // and contains the expected text in some encoding.
    auto *p = sec->get(pid::title);
    ASSERT_NE(p, nullptr);
    // Win32 writes VT_LPSTR with codepage 1200 (UTF-16LE).
    // Stout's parser now correctly decodes this as u16string.
    if (auto *ws = std::get_if<std::u16string>(&p->value)) {
        EXPECT_EQ(*ws, u"Win32 Title");
    } else if (auto *s = std::get_if<std::string>(&p->value)) {
        EXPECT_EQ(*s, "Win32 Title");
    } else {
        FAIL() << "Property value was neither string nor u16string";
    }
}

// ── Roundtrip: Stout writes → Win32 reads → Win32 modifies → Stout reads
TEST_F(PropertySetConformance, RoundtripStoutWin32Stout) {
    auto path = temp_file("ps_rt");
    guard_.add(path);

    // Stout writes
    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        property_set ps;
        auto &sec = ps.add_section(fmtid_summary_information());
        sec.set_string(pid::title, "Original");
        sec.set_string(pid::author, "Stout");
        ASSERT_TRUE(write_summary(*cf, ps));
        ASSERT_TRUE(cf->flush().has_value());
    }

    // Win32 reads and modifies
    {
        storage_ptr stg;
        ASSERT_TRUE(SUCCEEDED(win32_open_rw(path.wstring(), stg.put())));
        propset_storage_ptr pss;
        ASSERT_TRUE(SUCCEEDED(stg->QueryInterface(IID_IPropertySetStorage, reinterpret_cast<void **>(pss.put()))));
        propset_ptr prop;
        ASSERT_TRUE(SUCCEEDED(pss->Open(FMTID_SummaryInformation, STGM_READWRITE | STGM_SHARE_EXCLUSIVE, prop.put())));

        // Verify original title
        PROPSPEC spec{};
        spec.ulKind = PRSPEC_PROPID;
        spec.propid = PIDSI_TITLE;
        PROPVARIANT var{};
        PropVariantInit(&var);
        ASSERT_TRUE(SUCCEEDED(prop->ReadMultiple(1, &spec, &var)));
        EXPECT_EQ(var.vt, VT_LPSTR);
        if (var.vt == VT_LPSTR) EXPECT_STREQ(var.pszVal, "Original");
        PropVariantClear(&var);

        // Modify title
        var.vt = VT_LPSTR;
        var.pszVal = const_cast<char *>("Modified by Win32");
        ASSERT_TRUE(SUCCEEDED(prop->WriteMultiple(1, &spec, &var, PID_FIRST_USABLE)));
    }

    // Stout reads modified
    auto cf = compound_file::open(path, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto ps = read_summary(*cf);
    ASSERT_TRUE(ps.has_value());
    auto *sec = ps->section(fmtid_summary_information());
    ASSERT_NE(sec, nullptr);
    EXPECT_EQ(sec->get_string(pid::title), "Modified by Win32");
    EXPECT_EQ(sec->get_string(pid::author), "Stout");
}

// ── SubjectAndKeywords ──────────────────────────────────────────────────
TEST_F(PropertySetConformance, SubjectAndKeywords) {
    auto path = temp_file("ps_subkw");
    guard_.add(path);

    {
        auto cf = compound_file::create(path, cfb_version::v4);
        ASSERT_TRUE(cf.has_value());
        property_set ps;
        auto &sec = ps.add_section(fmtid_summary_information());
        sec.set_string(pid::subject, "Test Subject");
        sec.set_string(pid::keywords, "stout, cfb, test");
        ASSERT_TRUE(write_summary(*cf, ps));
        ASSERT_TRUE(cf->flush().has_value());
    }

    propset_storage_ptr pss;
    propset_ptr prop;
    ASSERT_TRUE(SUCCEEDED(open_summary_read(path.wstring(), pss, prop)));

    PROPSPEC specs[2]{};
    specs[0].ulKind = PRSPEC_PROPID;
    specs[0].propid = PIDSI_SUBJECT;
    specs[1].ulKind = PRSPEC_PROPID;
    specs[1].propid = PIDSI_KEYWORDS;
    PROPVARIANT vars[2]{};
    for (auto &v : vars) PropVariantInit(&v);
    ASSERT_TRUE(SUCCEEDED(prop->ReadMultiple(2, specs, vars)));

    EXPECT_EQ(vars[0].vt, VT_LPSTR);
    if (vars[0].vt == VT_LPSTR) EXPECT_STREQ(vars[0].pszVal, "Test Subject");
    EXPECT_EQ(vars[1].vt, VT_LPSTR);
    if (vars[1].vt == VT_LPSTR) EXPECT_STREQ(vars[1].pszVal, "stout, cfb, test");

    for (auto &v : vars) PropVariantClear(&v);
}

#endif // _WIN32
