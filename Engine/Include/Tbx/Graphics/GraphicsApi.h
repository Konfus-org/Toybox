#pragma once

namespace Tbx
{
    enum class TBX_EXPORT GraphicsApi
    {
        None,
        Vulkan,
        OpenGL
    };

    using ProcAddress = void*;

    class TBX_EXPORT IGLProcAddressProvider
    {
        virtual ~IGLProcAddressProvider() = default;
        virtual ProcAddress Provide() = 0;
    };
}
