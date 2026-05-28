#pragma once
#include "tbx/tbx_api.h"
#include <format>
#include <string>
#include <string_view>

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
    [[tbx::printable("{}", tbx::to_string($))]];
    enum class GraphicsApi
    {
        NONE,
        VULKAN,
        OPEN_GL,
        DIRECT_X,
        METAL,
        CUSTOM
    };

    inline std::string_view to_string(GraphicsApi api)
    {
        switch (api)
        {
            case GraphicsApi::NONE:
                return "None";
            case GraphicsApi::VULKAN:
                return "Vulkan";
            case GraphicsApi::OPEN_GL:
                return "OpenGL";
            case GraphicsApi::DIRECT_X:
                return "DirectX";
            case GraphicsApi::METAL:
                return "Metal";
            case GraphicsApi::CUSTOM:
                return "Custom";
            default:
                break;
        }

        return "Unknown";
    }
}

#include "tbx/systems/graphics/api.generated.h"
