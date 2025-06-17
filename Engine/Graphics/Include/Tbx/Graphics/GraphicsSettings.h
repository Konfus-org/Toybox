#pragma once
#include "Tbx/Utils/DllExport.h"
#include "Tbx/Graphics/Color.h"
#include "Tbx/Math/Size.h"

namespace Tbx
{
    enum class GraphicsApi
    {
        None,
        Vulkan,
        Metal,
        DirectX12
    };

    struct EXPORT GraphicsSettings
    {
        bool VSyncEnabled = true;
        GraphicsApi Api = GraphicsApi::Vulkan;
        Size Resolution = Size(800, 600);
        Color ClearColor = Color(0.1f, 0.1f, 0.1f, 1.0f);
    };
}
