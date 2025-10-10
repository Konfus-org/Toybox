#pragma once
#include "Tbx/DllExport.h"

namespace Tbx
{
    /// <summary>
    /// Represents the available vertical sync modes for a graphics context swap chain.
    /// </summary>
    enum class VsyncMode
    {
        Off,
        On,
        Adaptive
    };

    /// <summary>
    /// Represents the available graphics APIs.
    /// </summary>
    enum class TBX_EXPORT GraphicsApi
    {
        None,
        Vulkan,
        OpenGL,
        Metal,
        Custom
    };
}