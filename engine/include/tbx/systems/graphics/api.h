#pragma once
#include "tbx/systems/graphics/api.generated.h"

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
    [[printable]];
    enum class GraphicsApi
    {
        NONE [[name("none")]],
        VULKAN [[name("vulkan")]],
        OPEN_GL [[name("opengl")]],
        DIRECT_X [[name("directx")]],
        METAL [[name("metal")]],
        CUSTOM [[name("custom")]]
    };
}
