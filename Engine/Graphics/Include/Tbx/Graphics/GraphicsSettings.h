#pragma once
#include "Tbx/Utils/DllExport.h"
#include "Tbx/Graphics/Color.h"
#include "Tbx/Math/Size.h"

namespace Tbx
{
    enum class GraphicsApi
    {
        Vulkan = 0,
#ifdef TBX_PLATFORM_WINDOWS
        DX12
#endif
    };

    struct EXPORT GraphicsSettings
    {
        bool VSyncEnabled = true;
        GraphicsApi Api = GraphicsApi::Vulkan;
        Size Resolution = Size(800, 600);
        Color ClearColor = Color(0.1f, 0.1f, 0.1f, 1.0f);
    };
}
