#pragma once
#include "Tbx/DllExport.h"
#include <chrono>

namespace Tbx
{
    class TBX_EXPORT DeltaTime
    {
    public:
        /// <summary>
        /// Gets delta time in seconds
        /// </summary>
        static float InSeconds();

        /// <summary>
        /// Gets delta time in milliseconds
        /// </summary>
        static float InMilliseconds();

        /// <summary>
        /// Updates delta time.
        /// Should be called once per frame!
        /// </summary>
        static void Update();

    private:
        static float _valueInSeconds;
        static std::chrono::high_resolution_clock::time_point _lastFrameTime;
    };
}