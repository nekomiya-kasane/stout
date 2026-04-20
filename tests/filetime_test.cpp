#include "stout/util/filetime.h"

#include <gtest/gtest.h>

using namespace stout;
using namespace stout::util;

TEST(FiletimeTest, ZeroIsEpoch) {
    auto tp = filetime_to_timepoint(0);
    EXPECT_EQ(tp, file_time{});
}

TEST(FiletimeTest, EpochToFiletime) {
    auto ft = timepoint_to_filetime(file_time{});
    EXPECT_EQ(ft, 0u);
}

TEST(FiletimeTest, UnixEpoch) {
    // Unix epoch = 1970-01-01 = FILETIME 116444736000000000
    auto tp = filetime_to_timepoint(116444736000000000ULL);
    auto since_epoch = tp.time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(since_epoch).count();
    EXPECT_EQ(seconds, 0);
}

TEST(FiletimeTest, KnownDate) {
    // 2000-01-01 00:00:00 UTC = FILETIME 125911584000000000
    uint64_t ft_2000 = 125911584000000000ULL;
    auto tp = filetime_to_timepoint(ft_2000);
    auto ft_back = timepoint_to_filetime(tp);
    // Allow small rounding error due to duration_cast
    EXPECT_NEAR(static_cast<double>(ft_back), static_cast<double>(ft_2000), 100.0);
}

TEST(FiletimeTest, Roundtrip) {
    uint64_t original = 132500000000000000ULL; // some arbitrary FILETIME
    auto tp = filetime_to_timepoint(original);
    auto restored = timepoint_to_filetime(tp);
    EXPECT_NEAR(static_cast<double>(restored), static_cast<double>(original), 100.0);
}

TEST(FiletimeTest, FiletimeIsZero) {
    EXPECT_TRUE(filetime_is_zero(0));
    EXPECT_FALSE(filetime_is_zero(1));
    EXPECT_FALSE(filetime_is_zero(116444736000000000ULL));
}
