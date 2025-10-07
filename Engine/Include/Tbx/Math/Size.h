#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Math/Int.h"

namespace Tbx
{
    struct TBX_EXPORT Size
    {
        uint Width = 0;
        uint Height = 0;

        float GetAspectRatio() const
        {
            return static_cast<float>(Width) / static_cast<float>(Height);
        }
    };
}