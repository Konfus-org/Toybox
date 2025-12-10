#pragma once
#include "tbx/common/string.h"
#include "tbx/tbx_api.h"
#include <chrono>
#include <string>

namespace tbx
{
    // Time delta between frames/updates.
    // Ownership: value type.
    // Thread-safety: value type; freely copyable.
    struct TBX_API DeltaTime
    {
        double seconds = 0.0;
        double milliseconds = 0.0;

        operator String() const
        {
            return std::to_string(seconds) + "s";
        }
    };

    // Simple per-thread timer to compute DeltaTime.
    // Thread-safety: Not thread-safe; use a separate instance per thread.
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

}
