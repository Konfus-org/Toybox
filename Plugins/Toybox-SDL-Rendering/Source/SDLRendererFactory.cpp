#include "SDLRendererFactory.h"
#include "SDLRenderer.h"
#include <SDL3/SDL.h>
#include <Tbx/Events/EventCoordinator.h>

namespace SDLRendering
{
    void SDLRendererFactory::OnLoad()
    {
    }

    void SDLRendererFactory::OnUnload()
    {
    }

    Tbx::IRenderer* SDLRendererFactory::New()
    {
        auto* renderer = new SDLRenderer();
        return renderer;
    }

    void SDLRendererFactory::Delete(Tbx::IRenderer* renderer)
    {
        delete renderer;
    }
}
