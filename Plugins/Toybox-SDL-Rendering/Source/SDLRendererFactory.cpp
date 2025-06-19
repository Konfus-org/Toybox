#include "SDLRendererFactory.h"

namespace SDLRendering
{
    void SDLRendererFactory::OnLoad()
    {
    }

    void SDLRendererFactory::OnUnload()
    {
    }

    std::shared_ptr<Tbx::IRenderer> SDLRendererFactory::Create(std::shared_ptr<Tbx::IRenderSurface> surface)
    {
        return nullptr;
    }
}
