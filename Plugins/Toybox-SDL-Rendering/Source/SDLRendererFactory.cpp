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

    void DeleteRenderer(SDLRenderer* renderer)
    {
        delete renderer;
    }

    std::shared_ptr<Tbx::IRenderer> SDLRendererFactory::Create(std::shared_ptr<Tbx::IRenderSurface> surface)
    {
        auto renderer = std::shared_ptr<SDLRenderer>(new SDLRenderer(), [](SDLRenderer* rendererToDelete) { DeleteRenderer(rendererToDelete); });
        renderer->Initialize(surface);
        return renderer;
    }
}
