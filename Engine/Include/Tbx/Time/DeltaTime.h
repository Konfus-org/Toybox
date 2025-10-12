#pragma once
#include "Tbx/DllExport.h"

namespace Tbx
{
    /// <summary>
    /// Represents the elapsed time between two frames.
    /// </summary>
    struct TBX_EXPORT DeltaTime
    {
        constexpr DeltaTime()
            : Seconds(0.0f)
            , Milliseconds(0.0f)
        {
        }

        explicit constexpr DeltaTime(float seconds)
            : Seconds(seconds)
            , Milliseconds(seconds * 1000.0f)
        {
        }

        static constexpr DeltaTime FromMilliseconds(float milliseconds)
        {
            return DeltaTime(milliseconds / 1000.0f, milliseconds);
        }

        const float Seconds;
        const float Milliseconds;

    private:
        constexpr DeltaTime(float seconds, float milliseconds)
            : Seconds(seconds)
            , Milliseconds(milliseconds)
        {
        }
    };
}
