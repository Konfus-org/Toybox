#pragma once
#include <TbxCore.h>

namespace OpenGLRendering
{
    class OpenGLRendererFactory : public Tbx::FactoryPlugin<Tbx::IRenderer>
    {
    public:
        Tbx::IRenderer* Create() override;
        void Destroy(Tbx::IRenderer* windowToDestroy) override;

        void OnLoad() override;
        void OnUnload() override;
    };
}