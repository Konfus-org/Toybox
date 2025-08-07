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

    void SDLWindowFactory::Delete(Tbx::IWindow* window)
    {
        delete window;
    }

    Tbx::IWindow* SDLWindowFactory::New()
    {
        auto* newWindow = new SDLWindow();
        return newWindow;
    }
}
