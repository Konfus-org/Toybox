#pragma once
#include <Tbx/Core/DllExport.h>
#include <Tbx/Core/Rendering/Color.h>
#include <Tbx/Core/Math/Size.h>

namespace Tbx
{
    struct EXPORT GraphicsSettings
    {
        bool VSyncEnabled = true;
        // TODO: to support resolution that is seperate from window size 
        // we need to render to a texture then blit to the window
        Size Resolution = Size(1280, 720);
        Color ClearColor = Color(0.0f, 0.0f, 0.0f, 1.0f);
    };
}
