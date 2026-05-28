#pragma once

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
    [[tbx::printable]];
    enum class GraphicsApi
    {
        NONE [[tbx::name("none")]],
        VULKAN [[tbx::name("vulkan")]],
        OPEN_GL [[tbx::name("opengl")]],
        DIRECT_X [[tbx::name("directx")]],
        METAL [[tbx::name("metal")]],
        CUSTOM [[tbx::name("custom")]]
    };
}

#include "tbx/systems/graphics/api.generated.h"
