#pragma once
#include "tbx/tbx_api.h"
#include <string>

namespace tbx
{
    /// @brief
    /// Purpose: Supplies a graphics API procedure loader for runtime symbol lookup.
    /// @details
    /// Ownership: Non-owning function pointer; the provider controls lifetime.
    /// Thread Safety: Safe to copy; invocation must follow backend thread requirements.
    using GraphicsProcAddress = void* (*)(const char*);

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
