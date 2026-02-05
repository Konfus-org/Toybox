#include "tbx/graphics/graphics_api.h"

namespace tbx
{
    std::string to_string(GraphicsApi api)
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
                return "Unknown";
        }
    }
}
