#pragma once
#include "Tbx/DllExport.h"
#include <chrono>

namespace Tbx::Time
{
    class DeltaTime
    {
    public:
        /// <summary>
        /// Gets delta time in seconds
        /// </summary>
        EXPORT static float InSeconds();

        /// <summary>
        /// Gets delta time in milliseconds
        /// </summary>
        EXPORT static float InMilliseconds();

        /// <summary>
        /// Updates delta time.
        /// Should be called once per frame!
        /// </summary>
        EXPORT static void DrawFrame();

    private:
        static float _valueInSeconds;
        static std::chrono::high_resolution_clock::time_point _lastFrameTime;
    };
}