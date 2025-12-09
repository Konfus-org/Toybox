#pragma once
#include "tbx/tbx_api.h"

namespace tbx
{
    // Represents the available vertical sync modes for a graphics context swap chain.
    enum class VsyncMode
    {
        Off,
        On,
        Adaptive
    };

    // Represents the available graphics APIs.
    enum class TBX_API GraphicsApi
    {
        None,
        Vulkan,
        OpenGL,
        Metal,
        Custom
    };
}
