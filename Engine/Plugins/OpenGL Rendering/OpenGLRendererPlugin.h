#pragma once
#include <TbxCore.h>

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