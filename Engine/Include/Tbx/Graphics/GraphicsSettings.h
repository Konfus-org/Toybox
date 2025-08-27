#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Graphics/Color.h"
#include "Tbx/Math/Size.h"

namespace Tbx
{
    enum class GraphicsApi
    {
        None,
        //Vulkan,
        OpenGL
    };

    struct EXPORT GraphicsSettings
    {
        bool VSyncEnabled = true;
        GraphicsApi Api = GraphicsApi::OpenGL;
        Size Resolution = Size(800, 600);
        Color ClearColor = Color(0.0f, 0.0f, 0.05f, 1.0f);
    };
}
