#include "SDLRendererFactory.h"
#include "SDLRenderer.h"
#include <SDL3/SDL.h>
#include <Tbx/Events/EventCoordinator.h>

namespace SDLRendering
{
    void SDLRendererFactory::OnLoad()
    {
        _createRendererRequestId = Tbx::EventCoordinator::Subscribe<Tbx::CreateRendererRequest>(TBX_BIND_FN(OnCreateRendererRequest));
    }

    void SDLRendererFactory::OnUnload()
    {
        Tbx::EventCoordinator::Unsubscribe<Tbx::CreateRendererRequest>(_createRendererRequestId);
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

    void SDLRendererFactory::OnCreateRendererRequest(Tbx::CreateRendererRequest& r)
    {
        r.SetResult(Create(r.GetSurfaceParam()));
        r.IsHandled = true;
    }
}
