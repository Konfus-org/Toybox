#pragma once
#include "OpenGLRenderer.h"
#include <Tbx/Core/Rendering/IRenderer.h>
#include <Tbx/Core/Plugins/RegisterPlugin.h>

namespace OpenGLRendering
{
    class OpenGLRendererPlugin : public Tbx::Plugin, public OpenGLRenderer
    {
    public:
        OpenGLRendererPlugin() = default;
        ~OpenGLRendererPlugin() final = default;

        void OnLoad() override;
        void OnUnload() override;
    };
}