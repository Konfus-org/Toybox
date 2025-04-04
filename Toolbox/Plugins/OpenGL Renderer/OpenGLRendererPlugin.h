#pragma once
#include "OpenGLRenderer.h"
#include <Tbx/Core/Rendering/IRenderer.h>
#include <Tbx/Core/Plugins/RegisterPlugin.h>

namespace OpenGLRendering
{
    class OpenGLRendererPlugin : public OpenGLRenderer, public Tbx::Plugin
    {
    public:
        void OnLoad() override;
        void OnUnload() override;
    };
}