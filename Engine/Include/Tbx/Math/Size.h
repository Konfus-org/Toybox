#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Math/Int.h"

namespace Tbx
{
    struct TBX_EXPORT Size
    {
        uint Width = 0;
        uint Height = 0;
    };

    inline float CalculateAspectRatio(Size size)
    {
       return static_cast<float>(size.Width) / static_cast<float>(size.Height);
    }
}