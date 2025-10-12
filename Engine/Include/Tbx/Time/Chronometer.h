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
        using SystemClock = std::chrono::system_clock;
        using Seconds = std::chrono::duration<float>;

        Chronometer();

        /// <summary>
        /// Samples the underlying clock and updates the stored timing information.
        /// The first tick after construction or reset reports zero elapsed time.
        /// </summary>
        void Tick();

        /// <summary>
        /// Clears the stored sample so the next tick reports zero elapsed time.
        /// </summary>
        void Reset();

        /// <summary>
        /// Returns the elapsed time captured during the most recent tick.
        /// </summary>
        Seconds GetDeltaTime() const;

        /// <summary>
        /// Returns the accumulated elapsed time since the chronometer was constructed or reset.
        /// </summary>
        Seconds GetAccumulatedTime() const;

        /// <summary>
        /// Returns the system time captured during the most recent tick.
        /// </summary>
        SystemClock::time_point GetSystemTime() const;

    private:
        Clock::time_point _lastSample;
        SystemClock::time_point _systemTime;
        Seconds _deltaTime;
        Seconds _accumulatedTime;
        bool _hasLastSample;
    };
}
