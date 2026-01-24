#include "tbx/graphics/graphics_api.h"

namespace tbx
{
    std::string to_string(GraphicsApi api)
    {
        switch (api)
        {
            case GraphicsApi::None:
                return "None";
            case GraphicsApi::Vulkan:
                return "Vulkan";
            case GraphicsApi::OpenGL:
                return "OpenGL";
            case GraphicsApi::DirectX:
                return "DirectX";
            case GraphicsApi::Metal:
                return "Metal";
            case GraphicsApi::Custom:
                return "Custom";
            default:
                return "Unknown";
        }
    }
}
