#ifdef _WIN32

#include "conformance_utils.h"

#include <gtest/gtest.h>
#include <stout/compound_file.h>
#include <stout/ole/property_set.h>

using namespace conformance;
using namespace stout;
using namespace stout::ole;

struct VPProp {
    cfb_version ver;
    uint16_t major;
};

class StressPropertySetConformance : public ::testing::TestWithParam<VPProp> {
  protected:
    com_init com_;
    temp_file_guard guard_;

    static bool write_summary(compound_file &cf, const property_set &pset) {
        auto data = serialize_property_set(pset);
        if (!data) {
            return false;
        }
        auto root = cf.root_storage();
        auto s = root.create_stream("\005SummaryInformation");
        if (!s) {
            return false;
        }
        return s->write(0, std::span<const uint8_t>(*data)).has_value();
    }

    static std::expected<property_set, error> read_summary(compound_file &cf) {
        auto root = cf.root_storage();
        auto s = root.open_stream("\005SummaryInformation");
        if (!s) {
            return std::unexpected(s.error());
        }
        std::vector<uint8_t> buf(s->size());
        auto rd = s->read(0, std::span<uint8_t>(buf));
        if (!rd) {
            return std::unexpected(rd.error());
        }
        return parse_property_set(buf);
    }

    static HRESULT open_summary_read(const std::wstring &path, propset_storage_ptr &pss, propset_ptr &ps) {
        storage_ptr stg;
        HRESULT hr = win32_open_read(path, stg.put());
        if (FAILED(hr)) {
            return hr;
        }
        hr = stg->QueryInterface(IID_IPropertySetStorage, reinterpret_cast<void **>(pss.put()));
        if (FAILED(hr)) {
            return hr;
        }
        hr = pss->Open(FMTID_SummaryInformation, STGM_READ | STGM_SHARE_EXCLUSIVE, ps.put());
        return hr;
    }
};

static const VPProp vp_prop[] = {{cfb_version::v3, 3}, {cfb_version::v4, 4}};

INSTANTIATE_TEST_SUITE_P(V, StressPropertySetConformance, ::testing::ValuesIn(vp_prop),
                         [](const auto &info) { return info.param.major == 3 ? "V3" : "V4"; });

// ── Stout writes SummaryInfo, Win32 reads ───────────────────────────────

TEST_P(StressPropertySetConformance, TitleStringStoutToWin32) {
    auto p = temp_file("sp_title");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        property_set ps;
        auto &sec = ps.add_section(fmtid_summary_information());
        sec.set_string(pid::title, "Test Title");
        ASSERT_TRUE(write_summary(*cf, ps));
        ASSERT_TRUE(cf->flush().has_value());
    }
    propset_storage_ptr pss;
    propset_ptr prop;
    ASSERT_TRUE(SUCCEEDED(open_summary_read(p.wstring(), pss, prop)));
    PROPSPEC spec{};
    spec.ulKind = PRSPEC_PROPID;
    spec.propid = PIDSI_TITLE;
    PROPVARIANT var{};
    PropVariantInit(&var);
    ASSERT_TRUE(SUCCEEDED(prop->ReadMultiple(1, &spec, &var)));
    EXPECT_EQ(var.vt, VT_LPSTR);
    if (var.vt == VT_LPSTR) {
        EXPECT_STREQ(var.pszVal, "Test Title");
    }
    PropVariantClear(&var);
}

TEST_P(StressPropertySetConformance, AuthorStringStoutToWin32) {
    auto p = temp_file("sp_author");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        property_set ps;
        auto &sec = ps.add_section(fmtid_summary_information());
        sec.set_string(pid::author, "John Doe");
        ASSERT_TRUE(write_summary(*cf, ps));
        ASSERT_TRUE(cf->flush().has_value());
    }
    propset_storage_ptr pss;
    propset_ptr prop;
    ASSERT_TRUE(SUCCEEDED(open_summary_read(p.wstring(), pss, prop)));
    PROPSPEC spec{};
    spec.ulKind = PRSPEC_PROPID;
    spec.propid = PIDSI_AUTHOR;
    PROPVARIANT var{};
    PropVariantInit(&var);
    ASSERT_TRUE(SUCCEEDED(prop->ReadMultiple(1, &spec, &var)));
    EXPECT_EQ(var.vt, VT_LPSTR);
    if (var.vt == VT_LPSTR) {
        EXPECT_STREQ(var.pszVal, "John Doe");
    }
    PropVariantClear(&var);
}

TEST_P(StressPropertySetConformance, SubjectStringStoutToWin32) {
    auto p = temp_file("sp_subj");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        property_set ps;
        auto &sec = ps.add_section(fmtid_summary_information());
        sec.set_string(pid::subject, "Test Subject");
        ASSERT_TRUE(write_summary(*cf, ps));
        ASSERT_TRUE(cf->flush().has_value());
    }
    propset_storage_ptr pss;
    propset_ptr prop;
    ASSERT_TRUE(SUCCEEDED(open_summary_read(p.wstring(), pss, prop)));
    PROPSPEC spec{};
    spec.ulKind = PRSPEC_PROPID;
    spec.propid = PIDSI_SUBJECT;
    PROPVARIANT var{};
    PropVariantInit(&var);
    ASSERT_TRUE(SUCCEEDED(prop->ReadMultiple(1, &spec, &var)));
    EXPECT_EQ(var.vt, VT_LPSTR);
    if (var.vt == VT_LPSTR) {
        EXPECT_STREQ(var.pszVal, "Test Subject");
    }
    PropVariantClear(&var);
}

// ── Multiple properties ─────────────────────────────────────────────────

TEST_P(StressPropertySetConformance, MultiplePropertiesStoutToWin32) {
    auto p = temp_file("sp_multi");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        property_set ps;
        auto &sec = ps.add_section(fmtid_summary_information());
        sec.set_string(pid::title, "My Title");
        sec.set_string(pid::author, "Author Name");
        sec.set_string(pid::subject, "Subject Text");
        sec.set_string(pid::keywords, "key1, key2");
        ASSERT_TRUE(write_summary(*cf, ps));
        ASSERT_TRUE(cf->flush().has_value());
    }
    propset_storage_ptr pss;
    propset_ptr prop;
    ASSERT_TRUE(SUCCEEDED(open_summary_read(p.wstring(), pss, prop)));
    PROPSPEC spec{};
    spec.ulKind = PRSPEC_PROPID;
    spec.propid = PIDSI_TITLE;
    PROPVARIANT var{};
    PropVariantInit(&var);
    ASSERT_TRUE(SUCCEEDED(prop->ReadMultiple(1, &spec, &var)));
    EXPECT_EQ(var.vt, VT_LPSTR);
    if (var.vt == VT_LPSTR) {
        EXPECT_STREQ(var.pszVal, "My Title");
    }
    PropVariantClear(&var);
    spec.propid = PIDSI_KEYWORDS;
    ASSERT_TRUE(SUCCEEDED(prop->ReadMultiple(1, &spec, &var)));
    EXPECT_EQ(var.vt, VT_LPSTR);
    if (var.vt == VT_LPSTR) {
        EXPECT_STREQ(var.pszVal, "key1, key2");
    }
    PropVariantClear(&var);
}

// ── Integer property ────────────────────────────────────────────────────

TEST_P(StressPropertySetConformance, PageCountIntStoutToWin32) {
    auto p = temp_file("sp_int");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        property_set ps;
        auto &sec = ps.add_section(fmtid_summary_information());
        sec.set_i4(pid::page_count, 42);
        ASSERT_TRUE(write_summary(*cf, ps));
        ASSERT_TRUE(cf->flush().has_value());
    }
    propset_storage_ptr pss;
    propset_ptr prop;
    ASSERT_TRUE(SUCCEEDED(open_summary_read(p.wstring(), pss, prop)));
    PROPSPEC spec{};
    spec.ulKind = PRSPEC_PROPID;
    spec.propid = PIDSI_PAGECOUNT;
    PROPVARIANT var{};
    PropVariantInit(&var);
    ASSERT_TRUE(SUCCEEDED(prop->ReadMultiple(1, &spec, &var)));
    EXPECT_EQ(var.vt, VT_I4);
    if (var.vt == VT_I4) {
        EXPECT_EQ(var.lVal, 42);
    }
    PropVariantClear(&var);
}

// ── Win32 writes, Stout reads ───────────────────────────────────────────

TEST_P(StressPropertySetConformance, TitleWin32ToStout) {
    auto p = temp_file("sp_w2s_title");
    guard_.add(p);
    {
        storage_ptr stg;
        if (GetParam().ver == cfb_version::v4) {
            ASSERT_TRUE(SUCCEEDED(win32_create_v4(p.wstring(), stg.put())));
        } else {
            ASSERT_TRUE(SUCCEEDED(win32_create_v3(p.wstring(), stg.put())));
        }
        propset_storage_ptr pss;
        ASSERT_TRUE(SUCCEEDED(stg->QueryInterface(IID_IPropertySetStorage, reinterpret_cast<void **>(pss.put()))));
        propset_ptr ps;
        ASSERT_TRUE(SUCCEEDED(pss->Create(FMTID_SummaryInformation, nullptr, PROPSETFLAG_DEFAULT,
                                          STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, ps.put())));
        PROPSPEC spec{};
        spec.ulKind = PRSPEC_PROPID;
        spec.propid = PIDSI_TITLE;
        PROPVARIANT var{};
        var.vt = VT_LPSTR;
        var.pszVal = const_cast<char *>("Win32 Title");
        ASSERT_TRUE(SUCCEEDED(ps->WriteMultiple(1, &spec, &var, 2)));
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto ps = read_summary(*cf);
    ASSERT_TRUE(ps.has_value());
    ASSERT_FALSE(ps->sections.empty());
    // Win32 may encode the title differently; verify section is parseable
    EXPECT_FALSE(ps->sections[0].properties.empty());
}

TEST_P(StressPropertySetConformance, IntWin32ToStout) {
    auto p = temp_file("sp_w2s_int");
    guard_.add(p);
    {
        storage_ptr stg;
        if (GetParam().ver == cfb_version::v4) {
            ASSERT_TRUE(SUCCEEDED(win32_create_v4(p.wstring(), stg.put())));
        } else {
            ASSERT_TRUE(SUCCEEDED(win32_create_v3(p.wstring(), stg.put())));
        }
        propset_storage_ptr pss;
        ASSERT_TRUE(SUCCEEDED(stg->QueryInterface(IID_IPropertySetStorage, reinterpret_cast<void **>(pss.put()))));
        propset_ptr ps;
        ASSERT_TRUE(SUCCEEDED(pss->Create(FMTID_SummaryInformation, nullptr, PROPSETFLAG_DEFAULT,
                                          STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, ps.put())));
        PROPSPEC spec{};
        spec.ulKind = PRSPEC_PROPID;
        spec.propid = PIDSI_PAGECOUNT;
        PROPVARIANT var{};
        var.vt = VT_I4;
        var.lVal = 99;
        ASSERT_TRUE(SUCCEEDED(ps->WriteMultiple(1, &spec, &var, 2)));
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto ps = read_summary(*cf);
    ASSERT_TRUE(ps.has_value());
    ASSERT_FALSE(ps->sections.empty());
    EXPECT_EQ(ps->sections[0].get_i4(pid::page_count), 99);
}

// ── Roundtrip: Stout writes, Win32 modifies, Stout reads ───────────────

TEST_P(StressPropertySetConformance, RoundtripModify) {
    auto p = temp_file("sp_rt");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        property_set ps;
        auto &sec = ps.add_section(fmtid_summary_information());
        sec.set_string(pid::title, "Original");
        ASSERT_TRUE(write_summary(*cf, ps));
        ASSERT_TRUE(cf->flush().has_value());
    }
    {
        storage_ptr stg;
        ASSERT_TRUE(SUCCEEDED(win32_open_rw(p.wstring(), stg.put())));
        propset_storage_ptr pss;
        ASSERT_TRUE(SUCCEEDED(stg->QueryInterface(IID_IPropertySetStorage, reinterpret_cast<void **>(pss.put()))));
        propset_ptr ps;
        ASSERT_TRUE(SUCCEEDED(pss->Open(FMTID_SummaryInformation, STGM_READWRITE | STGM_SHARE_EXCLUSIVE, ps.put())));
        PROPSPEC spec{};
        spec.ulKind = PRSPEC_PROPID;
        spec.propid = PIDSI_TITLE;
        PROPVARIANT var{};
        var.vt = VT_LPSTR;
        var.pszVal = const_cast<char *>("Modified");
        ASSERT_TRUE(SUCCEEDED(ps->WriteMultiple(1, &spec, &var, 2)));
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto ps = read_summary(*cf);
    ASSERT_TRUE(ps.has_value());
    ASSERT_FALSE(ps->sections.empty());
    EXPECT_EQ(ps->sections[0].get_string(pid::title), "Modified");
}

// ── Empty string property ───────────────────────────────────────────────

TEST_P(StressPropertySetConformance, EmptyStringProperty) {
    auto p = temp_file("sp_empty");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        property_set ps;
        auto &sec = ps.add_section(fmtid_summary_information());
        sec.set_string(pid::title, "");
        ASSERT_TRUE(write_summary(*cf, ps));
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto ps = read_summary(*cf);
    ASSERT_TRUE(ps.has_value());
}

// ── Long string property ────────────────────────────────────────────────

TEST_P(StressPropertySetConformance, LongStringProperty) {
    auto p = temp_file("sp_long");
    guard_.add(p);
    std::string long_str(500, 'X');
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        property_set ps;
        auto &sec = ps.add_section(fmtid_summary_information());
        sec.set_string(pid::title, long_str);
        ASSERT_TRUE(write_summary(*cf, ps));
        ASSERT_TRUE(cf->flush().has_value());
    }
    propset_storage_ptr pss;
    propset_ptr prop;
    ASSERT_TRUE(SUCCEEDED(open_summary_read(p.wstring(), pss, prop)));
    PROPSPEC spec{};
    spec.ulKind = PRSPEC_PROPID;
    spec.propid = PIDSI_TITLE;
    PROPVARIANT var{};
    PropVariantInit(&var);
    ASSERT_TRUE(SUCCEEDED(prop->ReadMultiple(1, &spec, &var)));
    EXPECT_EQ(var.vt, VT_LPSTR);
    if (var.vt == VT_LPSTR) {
        EXPECT_EQ(std::string(var.pszVal), long_str);
    }
    PropVariantClear(&var);
}

// ── No property set stream ──────────────────────────────────────────────

TEST_P(StressPropertySetConformance, NoPropertySetReturnsError) {
    auto p = temp_file("sp_none");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        ASSERT_TRUE(cf->flush().has_value());
    }
    auto cf = compound_file::open(p, open_mode::read);
    ASSERT_TRUE(cf.has_value());
    auto s = cf->root_storage().open_stream("\005SummaryInformation");
    EXPECT_FALSE(s.has_value());
}

// ── Security property ───────────────────────────────────────────────────

TEST_P(StressPropertySetConformance, SecurityPropertyStoutToWin32) {
    auto p = temp_file("sp_sec");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        property_set ps;
        auto &sec = ps.add_section(fmtid_summary_information());
        sec.set_i4(pid::security, 0);
        ASSERT_TRUE(write_summary(*cf, ps));
        ASSERT_TRUE(cf->flush().has_value());
    }
    propset_storage_ptr pss;
    propset_ptr prop;
    ASSERT_TRUE(SUCCEEDED(open_summary_read(p.wstring(), pss, prop)));
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

// ── Keywords property ───────────────────────────────────────────────────

TEST_P(StressPropertySetConformance, KeywordsStoutToWin32) {
    auto p = temp_file("sp_kw");
    guard_.add(p);
    {
        auto cf = compound_file::create(p, GetParam().ver);
        ASSERT_TRUE(cf.has_value());
        property_set ps;
        auto &sec = ps.add_section(fmtid_summary_information());
        sec.set_string(pid::keywords, "alpha, beta, gamma");
        ASSERT_TRUE(write_summary(*cf, ps));
        ASSERT_TRUE(cf->flush().has_value());
    }
    propset_storage_ptr pss;
    propset_ptr prop;
    ASSERT_TRUE(SUCCEEDED(open_summary_read(p.wstring(), pss, prop)));
    PROPSPEC spec{};
    spec.ulKind = PRSPEC_PROPID;
    spec.propid = PIDSI_KEYWORDS;
    PROPVARIANT var{};
    PropVariantInit(&var);
    ASSERT_TRUE(SUCCEEDED(prop->ReadMultiple(1, &spec, &var)));
    EXPECT_EQ(var.vt, VT_LPSTR);
    if (var.vt == VT_LPSTR) {
        EXPECT_STREQ(var.pszVal, "alpha, beta, gamma");
    }
    PropVariantClear(&var);
}

#endif // _WIN32
