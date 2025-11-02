#pragma once
#include "tbx/tbx_api.h"
#include <chrono>

namespace tbx
{
    struct TBX_API DeltaTime
    {
        double seconds = 0.0;
        double milliseconds = 0.0;
    };

    class TBX_API DeltaTimer
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

    inline std::string to_string(const DeltaTime& dt)
    {
        return std::to_string(dt.seconds) + "s";
    }
}
