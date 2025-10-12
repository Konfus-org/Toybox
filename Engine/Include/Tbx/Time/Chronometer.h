#pragma once
#include "Tbx/DllExport.h"
#include <chrono>

namespace Tbx
{
    /// <summary>
    /// Utility for measuring the elapsed time between samples.
    /// </summary>
    class TBX_EXPORT Chronometer
    {
    public:
        using Clock = std::chrono::high_resolution_clock;
        using Seconds = std::chrono::duration<float>;

        Chronometer();

        /// <summary>
        /// Samples the underlying clock and returns the elapsed time since the previous tick.
        /// The first tick after construction or reset reports zero elapsed time.
        /// </summary>
        template <typename Duration = Seconds>
        Duration Tick()
        {
            const auto now = Clock::now();

            if (!_hasLastSample)
            {
                _hasLastSample = true;
                _lastSample = now;
                return Duration::zero();
            }

            const auto elapsed = now - _lastSample;
            _lastSample = now;
            return std::chrono::duration_cast<Duration>(elapsed);
        }

        /// <summary>
        /// Clears the stored sample so the next tick reports zero elapsed time.
        /// </summary>
        void Reset();

    private:
        Clock::time_point _lastSample;
        bool _hasLastSample;
    };
}
