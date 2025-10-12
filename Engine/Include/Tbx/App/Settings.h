#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Graphics/Color.h"
#include "Tbx/Graphics/GraphicsApi.h"
#include "Tbx/Math/Size.h"

namespace Tbx
{
    /// <summary>
    /// Configures runtime behaviour for the core application such as display resolution and rendering backend.
    /// </summary>
    struct TBX_EXPORT AppSettings
    {
        VsyncMode Vsync = VsyncMode::Off;
        GraphicsApi RenderingApi = GraphicsApi::OpenGL;
        Size Resolution = { static_cast<uint>(800), static_cast<uint>(600) };
        RgbaColor ClearColor = RgbaColor(0.0f, 0.0f, 0.05f, 1.0f);
    };
}
