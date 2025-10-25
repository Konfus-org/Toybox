#pragma once
#include <chrono>

namespace tbx
{
    struct DeltaTime
    {
        double seconds = 0.0;
        double milliseconds = 0.0;
    };

    class DeltaTimer
    {
    public:
        DeltaTimer();

        // Resets internal state and starts timing from now
        void reset();

        // Advances the timer and returns the time since the previous tick
        DeltaTime tick();

    private:
        std::chrono::steady_clock::time_point _last;
    };
}
