#pragma once
#include "Tbx/DllExport.h"
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

}
