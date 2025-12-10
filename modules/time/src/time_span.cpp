#include "tbx/time/time_span.h"

namespace tbx
{
    TimeSpan::operator bool() const
    {
        return value != 0;
    }

    TimeSpan::operator std::chrono::steady_clock::duration() const
    {
        return to_duration();
    }

    TimeSpan::operator int() const
    {
        return static_cast<int>(value);
    }

    bool TimeSpan::is_zero() const
    {
        return value == 0;
    }

    std::chrono::steady_clock::duration TimeSpan::to_duration() const
    {
        std::chrono::steady_clock::duration duration = {};

        switch (unit)
        {
            case TimeUnit::Milliseconds:
                duration = std::chrono::milliseconds(value);
                break;
            case TimeUnit::Seconds:
                duration = std::chrono::seconds(value);
                break;
            case TimeUnit::Minutes:
                duration = std::chrono::minutes(value);
                break;
            case TimeUnit::Hours:
                duration = std::chrono::hours(value);
                break;
            case TimeUnit::Days:
                duration = std::chrono::hours(value * 24);
                break;
            default:
                break;
        }

        return duration;
    }
}
