#pragma once
#include "tbx/tbx_api.h"
#include "tbx/types/typedefs.h"
#include <chrono>

namespace tbx
{
    [[tbx::printable]];
    enum TimeUnit
    {
        MILLISECONDS [[tbx::name("ms")]],
        SECONDS [[tbx::name("s")]],
        MINUTES [[tbx::name("min")]],
        HOURS [[tbx::name("h")]],
        DAYS [[tbx::name("d")]]
    };

    // Value-type duration helper; thread-safe due to copy semantics.
    [[tbx::printable("{} {}", value, unit)]];
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

}

#include "tbx/systems/time/span.generated.h"
