#pragma once
#include <Tbx/Plugin API/PluginInterfaces.h>

namespace SDLRendering
{
    class SDLRendererFactory : public Tbx::IRendererFactoryPlugin
    {
    public:
        void OnLoad() override;
        void OnUnload() override;

        std::shared_ptr<Tbx::IRenderer> Create(std::shared_ptr<Tbx::IRenderSurface> surface) override;
    };
}
