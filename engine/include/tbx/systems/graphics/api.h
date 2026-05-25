#pragma once
#include "tbx/tbx_api.h"
#include <format>
#include <string>

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

}

template <>
struct std::formatter<tbx::GraphicsApi>
{
    constexpr auto parse(std::format_parse_context& ctx)
    {
        return _formatter.parse(ctx);
    }

    template <typename TFormatContext>
    auto format(tbx::GraphicsApi api, TFormatContext& ctx) const
    {
        auto name = std::string_view("Unknown");
        switch (api)
        {
            case tbx::GraphicsApi::NONE:
                name = "None";
                break;
            case tbx::GraphicsApi::VULKAN:
                name = "Vulkan";
                break;
            case tbx::GraphicsApi::OPEN_GL:
                name = "OpenGL";
                break;
            case tbx::GraphicsApi::DIRECT_X:
                name = "DirectX";
                break;
            case tbx::GraphicsApi::METAL:
                name = "Metal";
                break;
            case tbx::GraphicsApi::CUSTOM:
                name = "Custom";
                break;
            default:
                break;
        }

        return _formatter.format(std::string(name), ctx);
    }

    std::formatter<std::string> _formatter;
};
