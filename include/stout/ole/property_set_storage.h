#pragma once

#include "stout/exports.h"
#include "stout/compound_file.h"
#include "stout/ole/property_set.h"
#include <expected>

namespace stout::ole {

// Well-known property stream names (UTF-8)
inline constexpr const char* summary_info_stream      = "\005SummaryInformation";
inline constexpr const char* doc_summary_info_stream   = "\005DocumentSummaryInformation";

// ── Read / Write property sets from a compound file storage ───────────

// Read a property set from a named stream within a storage.
// The stream name should be one of the well-known names above.
[[nodiscard]] STOUT_API auto read_property_set(storage& stg, std::string_view stream_name)
    -> std::expected<property_set, error>;

// Write a property set to a named stream within a storage.
// Creates the stream if it doesn't exist, overwrites if it does.
STOUT_API auto write_property_set(storage& stg, std::string_view stream_name,
                                   const property_set& ps)
    -> std::expected<void, error>;

// ── Convenience: SummaryInformation ───────────────────────────────────

// Read the \005SummaryInformation property set from a storage.
[[nodiscard]] STOUT_API auto read_summary_info(storage& stg)
    -> std::expected<property_set, error>;

// Write the \005SummaryInformation property set to a storage.
STOUT_API auto write_summary_info(storage& stg, const property_set& ps)
    -> std::expected<void, error>;

// ── Convenience: DocumentSummaryInformation ───────────────────────────

// Read the \005DocumentSummaryInformation property set from a storage.
[[nodiscard]] STOUT_API auto read_doc_summary_info(storage& stg)
    -> std::expected<property_set, error>;

// Write the \005DocumentSummaryInformation property set to a storage.
STOUT_API auto write_doc_summary_info(storage& stg, const property_set& ps)
    -> std::expected<void, error>;

// ── Convenience builders ──────────────────────────────────────────────

// Create a minimal SummaryInformation property set with common fields.
[[nodiscard]] STOUT_API auto make_summary_info(
    std::string_view title   = {},
    std::string_view subject = {},
    std::string_view author  = {},
    std::string_view app_name = {}
) -> property_set;

// ── Property value helpers ────────────────────────────────────────────

// Get a human-readable type name for a VT type
[[nodiscard]] STOUT_API auto vt_type_name(vt type) noexcept -> const char*;

// Get a debug string representation of a property value
[[nodiscard]] STOUT_API auto property_value_to_string(const property& prop) -> std::string;

} // namespace stout::ole
