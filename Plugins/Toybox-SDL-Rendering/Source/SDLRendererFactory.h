#pragma once
#include <Tbx/Plugin API/RegisterPlugin.h>
#include <Tbx/Events/RenderEvents.h>

namespace SDLRendering
{
    class SDLRendererFactory : public Tbx::IRendererFactoryPlugin
    {
    public:
        void OnLoad() override;
        void OnUnload() override;

    protected:
        Tbx::IRenderer* New() override;
        void Delete(Tbx::IRenderer* renderer) override;
    };

    TBX_REGISTER_PLUGIN(SDLRendererFactory);
}
