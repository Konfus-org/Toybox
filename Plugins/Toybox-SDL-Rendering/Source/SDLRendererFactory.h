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

        std::shared_ptr<Tbx::IRenderer> Create(std::shared_ptr<Tbx::IRenderSurface> surface) override;
    };

    TBX_REGISTER_PLUGIN(SDLRendererFactory);
}
