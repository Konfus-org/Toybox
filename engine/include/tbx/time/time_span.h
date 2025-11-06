#pragma once
#include "tbx/tbx_api.h"
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

        TBX_API std::chrono::steady_clock::duration to_duration() const;
        TBX_API bool is_zero() const;
    };

    inline std::string to_string(const TimeSpan& span)
    {
        return std::to_string(span.milliseconds) + " milliseconds";
    }
}
