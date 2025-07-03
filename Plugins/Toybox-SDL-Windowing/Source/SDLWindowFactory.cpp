#include "SDLWindowFactory.h"
#include "SDLWindow.h"
#include <Tbx/Events/EventCoordinator.h>

namespace SDLWindowing
{
    void SDLWindowFactory::OnLoad()
    {
    }

    void SDLWindowFactory::OnUnload()
    {
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
}
