#pragma once

#include <chrono>
#include <cstdint>

namespace tbx
{
    // Value-type duration helper; thread-safe due to copy semantics.
    struct TimeSpan
    {
        std::int64_t milliseconds = 0;
        std::int64_t seconds = 0;
        std::int64_t minutes = 0;
        std::int64_t hours = 0;
        std::int64_t days = 0;

        std::chrono::steady_clock::duration to_duration() const;
        bool is_zero() const;
    };
}

