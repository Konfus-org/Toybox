#include "tbx/time/time_span.h"

namespace tbx
{
    std::chrono::steady_clock::duration TimeSpan::to_duration() const
    {
        std::chrono::steady_clock::duration duration{};

        duration += std::chrono::milliseconds(milliseconds);
        duration += std::chrono::seconds(seconds);
        duration += std::chrono::minutes(minutes);
        duration += std::chrono::hours(hours);

        if (days != 0)
        {
            duration += std::chrono::hours(days * 24);
        }

        return duration;
    }

    bool TimeSpan::is_zero() const
    {
        return milliseconds == 0 && seconds == 0 && minutes == 0 && hours == 0 && days == 0;
    }
}

