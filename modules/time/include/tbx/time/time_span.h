#pragma once
#include "tbx/common/int.h"
#include "tbx/common/string.h"
#include "tbx/tbx_api.h"
#include <chrono>
#include <cstdint>
#include <string>

namespace tbx
{
    enum TimeUnit
    {
        Milliseconds,
        Seconds,
        Minutes,
        Hours,
        Days
    };

    // Value-type duration helper; thread-safe due to copy semantics.
    struct TBX_API TimeSpan
    {
        bool is_zero() const;
        std::chrono::steady_clock::duration to_duration() const;

        // Implicit conversion to bool; true if non-zero duration.
        operator bool() const;

        // Implicit conversion to std::chrono::steady_clock::duration.
        operator std::chrono::steady_clock::duration() const;

        // Implicit conversion to total value.
        operator int() const;

        operator String() const
        {
            switch (unit)
            {
                case TimeUnit::Milliseconds:
                    return std::to_string(value) + " ms";
                case TimeUnit::Seconds:
                    return std::to_string(value) + " s";
                case TimeUnit::Minutes:
                    return std::to_string(value) + " min";
                case TimeUnit::Hours:
                    return std::to_string(value) + " h";
                case TimeUnit::Days:
                    return std::to_string(value) + " d";
                default:
                    return std::to_string(value) + " (unknown unit)";
            }
        }

        uint64 value = 0;
        TimeUnit unit = TimeUnit::Milliseconds;
    };
}
