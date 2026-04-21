#include "stout/ole/property_set_storage.h"

#include <vector>

namespace stout::ole {

    auto read_property_set(storage &stg, std::string_view stream_name) -> std::expected<property_set, error> {
        auto strm = stg.open_stream(stream_name);
        if (!strm) {
            return std::unexpected(strm.error());
        }

        auto sz = strm->size();
        if (sz == 0) {
            return std::unexpected(error::corrupt_file);
        }

        std::vector<uint8_t> buf(static_cast<size_t>(sz));
        auto read_r = strm->read(0, std::span<uint8_t>(buf));
        if (!read_r) {
            return std::unexpected(read_r.error());
        }

        return parse_property_set(std::span<const uint8_t>(buf.data(), static_cast<size_t>(*read_r)));
    }

    auto write_property_set(storage &stg, std::string_view stream_name, const property_set &ps)
        -> std::expected<void, error> {
        auto serialized = serialize_property_set(ps);
        if (!serialized) {
            return std::unexpected(serialized.error());
        }

        // Create or open the stream
        auto strm = stg.exists(stream_name) ? stg.open_stream(stream_name) : stg.create_stream(stream_name);
        if (!strm) {
            return std::unexpected(strm.error());
        }

        // Resize to fit the serialized data
        auto resize_r = strm->resize(serialized->size());
        if (!resize_r) {
            return std::unexpected(resize_r.error());
        }

        // Write the data
        auto write_r = strm->write(0, std::span<const uint8_t>(*serialized));
        if (!write_r) {
            return std::unexpected(write_r.error());
        }

        return {};
    }

    auto read_summary_info(storage &stg) -> std::expected<property_set, error> {
        return read_property_set(stg, summary_info_stream);
    }

    auto write_summary_info(storage &stg, const property_set &ps) -> std::expected<void, error> {
        return write_property_set(stg, summary_info_stream, ps);
    }

    auto read_doc_summary_info(storage &stg) -> std::expected<property_set, error> {
        return read_property_set(stg, doc_summary_info_stream);
    }

    auto write_doc_summary_info(storage &stg, const property_set &ps) -> std::expected<void, error> {
        return write_property_set(stg, doc_summary_info_stream, ps);
    }

} // namespace stout::ole
