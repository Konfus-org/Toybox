#include "SDLWindowFactory.h"
#include "SDLWindow.h"
#include <Tbx/Events/EventCoordinator.h>

namespace SDLWindowing
{
    void SDLWindowFactory::OnLoad()
    {
        _openCreateWindowRequestId = Tbx::EventCoordinator::Subscribe<Tbx::CreateWindowRequest>(TBX_BIND_FN(OnCreateWindowRequest));
    }

    void SDLWindowFactory::OnUnload()
    {
        Tbx::EventCoordinator::Unsubscribe<Tbx::CreateWindowRequest>(_openCreateWindowRequestId);
    }

    void DeleteWindow(SDLWindow* windowToDelete)
    {
        delete windowToDelete;
    }

    std::shared_ptr<Tbx::IWindow> SDLWindowFactory::Create(const std::string& title, const Tbx::Size& size)
    {
        auto* newWindow = new SDLWindow(title, size);
        return std::shared_ptr<SDLWindow>(newWindow, [](SDLWindow* windowToDelete) { DeleteWindow(windowToDelete); });
    }

    void SDLWindowFactory::OnCreateWindowRequest(Tbx::CreateWindowRequest& r)
    {
        r.SetResult(Create(r.GetNameParam(), r.GetSizeParam()));
        r.IsHandled = true;
    }
}
