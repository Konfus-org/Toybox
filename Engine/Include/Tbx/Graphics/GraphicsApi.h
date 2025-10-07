#pragma once
#include "Tbx/DllExport.h"

namespace Tbx
{
    enum class TBX_EXPORT GraphicsApi
    {
        None,
        Vulkan,
        OpenGL,
        Metal,
        Custom
    };
}