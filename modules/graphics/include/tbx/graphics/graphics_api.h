#pragma once
#include "tbx/tbx_api.h"

namespace tbx
{
    // Represents the available vertical sync modes for a graphics context swap chain.
    enum class VsyncMode
    {
        OFF,
        ON,
        ADAPTIVE
    };

    // Represents the available graphics APIs.
    enum class GraphicsApi
    {
        NONE,
        VULKAN,
        OPEN_GL,
        DIRECT_X,
        METAL,
        CUSTOM
    };

    // Converts a GraphicsApi enum value to its string representation.
    TBX_API std::string to_string(GraphicsApi api);
}
