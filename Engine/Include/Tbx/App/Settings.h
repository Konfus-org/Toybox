#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Graphics/Color.h"
#include "Tbx/Graphics/GraphicsApi.h"
#include "Tbx/Math/Size.h"

namespace Tbx
{
    struct EXPORT Settings
    {
        bool VSyncEnabled = true;
        GraphicsApi Api = GraphicsApi::OpenGL;
        Size Resolution = Size(800, 600);
        RgbaColor ClearColor = RgbaColor(0.0f, 0.0f, 0.05f, 1.0f);
    };
}
