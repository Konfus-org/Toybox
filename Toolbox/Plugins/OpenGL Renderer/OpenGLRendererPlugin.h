#pragma once
#include <Tbx/Core/Plugins/RegisterPlugin.h>
#include <Tbx/App/Render Pipeline/IRenderer.h>

namespace OpenGLRendering
{
    class OpenGLRendererPlugin : public Tbx::Plugin<Tbx::IRenderer>
    {
    public:
        Tbx::IRenderer* Provide() override;
        void Destroy(Tbx::IRenderer* windowToDestroy) override;

        void OnLoad() override;
        void OnUnload() override;
    };
}