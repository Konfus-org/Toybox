#pragma once
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
    };

    /// <summary>Purpose: Formats a DeltaTime as a human-readable string.</summary>
    /// <remarks>Ownership: Returns an owned std::string. Thread Safety: Stateless and safe for concurrent use.</remarks>
    TBX_API std::string to_string(const DeltaTime& delta_time);

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
