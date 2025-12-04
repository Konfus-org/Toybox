#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Math/Size.h"
#include "Tbx/Math/Vectors.h"

namespace Tbx
{
    struct TBX_EXPORT Viewport
    {
        Vector2 Position;
        Size Extends;
    };
}
