#pragma once
#include "tbx/tbx_api.h"
#include <chrono>
#include <cstdint>
#include <string>

namespace tbx
{
    // Value-type duration helper; thread-safe due to copy semantics.
    struct TBX_API TimeSpan
    {
        std::int64_t milliseconds = 0;
        std::int64_t seconds = 0;
        std::int64_t minutes = 0;
        std::int64_t hours = 0;
        std::int64_t days = 0;

        std::chrono::steady_clock::duration to_duration() const;
        bool is_zero() const;

        // Implicit conversion to bool; true if non-zero duration.
        operator bool() const
        {
            return !is_zero();
        }

        // Implicit conversion to std::chrono::steady_clock::duration.
        operator std::chrono::steady_clock::duration() const
        {
            return to_duration();
        }

        // Implicit conversion to total milliseconds.
        operator int() const
        {
            return milliseconds;
        }
    };

    inline std::string to_string(const TimeSpan& span)
    {
        return std::to_string(span.milliseconds) + " milliseconds";
    }
}
