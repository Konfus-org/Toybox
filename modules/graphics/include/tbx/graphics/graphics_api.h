#pragma once
#include "tbx/tbx_api.h"

namespace tbx
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
    enum class TBX_API GraphicsApi
    {
        None,
        Vulkan,
        OpenGL,
        Metal,
        Custom
    };
}