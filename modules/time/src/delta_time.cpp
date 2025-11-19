#include "tbx/time/delta_time.h"

namespace tbx
{
    DeltaTimer::DeltaTimer()
    {
        reset();
    }

    void DeltaTimer::reset()
    {
        _last = std::chrono::steady_clock::now();
    }

    DeltaTime DeltaTimer::tick()
    {
        const auto now = std::chrono::steady_clock::now();
        const auto delta = now - _last;
        _last = now;

        const double secs = std::chrono::duration<double>(delta).count();
        DeltaTime dt;
        dt.seconds = secs;
        dt.milliseconds = secs * 1000.0;
        return dt;
    }
}
