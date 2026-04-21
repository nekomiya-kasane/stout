#include "stout/util/filetime.h"

namespace stout::util {

auto filetime_to_timepoint(uint64_t ft) noexcept -> file_time {
    if (ft == 0) {
        return file_time{};
    }

    // Convert from 100ns intervals since 1601 to microseconds since Unix epoch
    int64_t intervals = static_cast<int64_t>(ft) - filetime_unix_epoch_diff;
    auto duration = std::chrono::duration<int64_t, std::ratio<1, 10'000'000>>(intervals);
    return file_time{std::chrono::duration_cast<std::chrono::system_clock::duration>(duration)};
}

auto timepoint_to_filetime(file_time tp) noexcept -> uint64_t {
    if (tp == file_time{}) {
        return 0;
    }

    auto since_epoch = tp.time_since_epoch();
    auto intervals = std::chrono::duration_cast<std::chrono::duration<int64_t, std::ratio<1, 10'000'000>>>(since_epoch);
    return static_cast<uint64_t>(intervals.count() + filetime_unix_epoch_diff);
}

} // namespace stout::util
