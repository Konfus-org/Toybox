#pragma once
#include "tbx/common/int.h"
#include "tbx/tbx_api.h"
#include <chrono>
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

        operator bool() const;
        operator int() const;
        operator std::string() const;
        operator std::chrono::steady_clock::duration() const;

        uint64 value = 0;
        TimeUnit unit = TimeUnit::Milliseconds;
    };
}
