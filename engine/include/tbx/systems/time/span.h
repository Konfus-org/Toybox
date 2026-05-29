#pragma once
#include "tbx/systems/time/span.generated.h"
#include "tbx/tbx_api.h"
#include "tbx/types/typedefs.h"
#include <chrono>

namespace tbx
{
    [[printable]];
    enum class TimeUnit
    {
        MILLISECONDS [[name("ms")]],
        SECONDS [[name("s")]],
        MINUTES [[name("min")]],
        HOURS [[name("h")]],
        DAYS [[name("d")]]
    };

    // Value-type duration helper; thread-safe due to copy semantics.
    [[printable("{} {}", value, unit)]];
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
