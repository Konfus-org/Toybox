#include "SDLWindowFactory.h"

namespace SDLWindowing
{
    void SDLWindowFactory::OnLoad()
    {
    }

    void SDLWindowFactory::OnUnload()
    {
    }

    std::shared_ptr<Tbx::IWindow> SDLWindowFactory::Create(const std::string& title, const Tbx::Size& size)
    {
        return nullptr;
    }
}
