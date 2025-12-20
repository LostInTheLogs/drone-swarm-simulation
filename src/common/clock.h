#pragma once

#include <chrono>
#include <ctime>

// NOLINTBEGIN(readability-identifier-naming)

struct MonotonicClock {
    using rep = std::chrono::nanoseconds::rep;
    using period = std::chrono::nanoseconds::period;
    using duration = std::chrono::nanoseconds;
    using time_point = std::chrono::time_point<MonotonicClock>;
    static constexpr bool is_steady = true;

    static auto now() noexcept -> time_point {
        timespec tspec{};
        if (clock_gettime(CLOCK_MONOTONIC, &tspec) != 0) {
            std::abort();
        }

        auto sec = std::chrono::seconds(tspec.tv_sec);
        auto nsec = std::chrono::nanoseconds(tspec.tv_nsec);

        return time_point(sec + nsec);
    }
};

// NOLINTEND(readability-identifier-naming)
