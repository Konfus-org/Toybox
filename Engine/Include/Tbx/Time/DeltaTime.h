#pragma once
#include "Tbx/DllExport.h"
#include <chrono>

namespace Tbx
{
    /// <summary>
    /// Represents the elapsed time between two frames.
    /// </summary>
    struct TBX_EXPORT DeltaTime
    {
        explicit DeltaTime(float seconds = 0.0f)
            : Seconds(seconds)
            , Milliseconds(seconds * 1000.0f)
        {
        }

        void SetSeconds(float seconds)
        {
            Seconds = seconds;
            Milliseconds = seconds * 1000.0f;
        }

        float Seconds = 0.0f;
        float Milliseconds = 0.0f;
    };

    /// <summary>
    /// Tracks frame timings and produces <see cref="DeltaTime"/> snapshots.
    /// </summary>
    class TBX_EXPORT DeltaClock
    {
    public:
        DeltaClock();

        /// <summary>
        /// Samples the current clock and returns the elapsed time since the last tick.
        /// The first tick after construction returns a zero delta.
        /// </summary>
        DeltaTime Tick();

        /// <summary>
        /// Resets the clock so the next tick returns a zero delta.
        /// </summary>
        void Reset();

    private:
        std::chrono::high_resolution_clock::time_point _lastFrameTime;
        bool _hasLastFrameTime;
    };
}
