#include "tbx/time/time_span.h"

namespace tbx
{

    bool TimeSpan::is_zero() const
    {
        return value == 0;
    }

    std::chrono::steady_clock::duration TimeSpan::to_duration() const
    {
        std::chrono::steady_clock::duration duration = {};

        switch (unit)
        {
            case TimeUnit::MILLISECONDS:
                duration = std::chrono::milliseconds(value);
                break;
            case TimeUnit::SECONDS:
                duration = std::chrono::seconds(value);
                break;
            case TimeUnit::MINUTES:
                duration = std::chrono::minutes(value);
                break;
            case TimeUnit::HOURS:
                duration = std::chrono::hours(value);
                break;
            case TimeUnit::DAYS:
                duration = std::chrono::hours(value * 24);
                break;
            default:
                break;
        }

        return duration;
    }

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

    std::string to_string(const TimeSpan& time_span)
    {
        switch (time_span.unit)
        {
            case TimeUnit::MILLISECONDS:
                return std::to_string(time_span.value) + " ms";
            case TimeUnit::SECONDS:
                return std::to_string(time_span.value) + " s";
            case TimeUnit::MINUTES:
                return std::to_string(time_span.value) + " min";
            case TimeUnit::HOURS:
                return std::to_string(time_span.value) + " h";
            case TimeUnit::DAYS:
                return std::to_string(time_span.value) + " d";
            default:
                return std::to_string(time_span.value) + " (unknown unit)";
        }
    }
}
