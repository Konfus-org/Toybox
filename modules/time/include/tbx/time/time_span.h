#pragma once
#include "tbx/common/int.h"
#include "tbx/tbx_api.h"
#include <chrono>
#include <string>

namespace tbx
{
    enum TimeUnit
    {
        MILLISECONDS,
        SECONDS,
        MINUTES,
        HOURS,
        DAYS
    };

    // Value-type duration helper; thread-safe due to copy semantics.
    struct TBX_API TimeSpan
    {
        bool is_zero() const;
        std::chrono::steady_clock::duration to_duration() const;

        operator bool() const;
        operator int() const;
        operator std::chrono::steady_clock::duration() const;

        uint64 value = 0;
        TimeUnit unit = TimeUnit::MILLISECONDS;
    };

    /// <summary>Purpose: Formats a TimeSpan value with its unit for display.</summary>
    /// <remarks>Ownership: Returns an owned std::string. Thread Safety: Stateless and safe for concurrent use.</remarks>
    TBX_API std::string to_string(const TimeSpan& time_span);
}
