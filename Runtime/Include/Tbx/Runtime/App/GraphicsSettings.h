#pragma once
#include <Tbx/Core/DllExport.h>
#include <Tbx/Core/Rendering/Color.h>
#include <Tbx/Core/Math/Size.h>

namespace Tbx
{
    struct EXPORT GraphicsSettings
    {
        bool VSyncEnabled = true;
        Size Resolution = Size(800, 600);
        Color ClearColor = Color(0.1f, 0.1f, 0.1f, 1.0f);
    };
}
