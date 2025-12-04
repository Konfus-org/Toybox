#pragma once
#include "Tbx/DllExport.h"

namespace Tbx
{
    /// <summary>
    /// Represents the elapsed time between two frames.
    /// </summary>
    struct TBX_EXPORT DeltaTime
    {
        static constexpr DeltaTime FromSeconds(float seconds)
        {
            return DeltaTime(seconds, seconds * 1000.0f);
        }

        static constexpr DeltaTime FromMilliseconds(float milliseconds)
        {
            return DeltaTime(milliseconds / 1000.0f, milliseconds);
        }

        const float Seconds = 0;
        const float Milliseconds = 0;
    };
}
