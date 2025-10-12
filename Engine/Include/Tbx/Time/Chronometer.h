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
        Seconds Tick();

        /// <summary>
        /// Clears the stored sample so the next tick reports zero elapsed time.
        /// </summary>
        void Reset();

    private:
        Clock::time_point _lastSample;
        bool _hasLastSample;
    };
}
