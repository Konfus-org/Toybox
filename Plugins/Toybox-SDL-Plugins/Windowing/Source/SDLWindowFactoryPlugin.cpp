#include "SDLWindowFactoryPlugin.h"
#include "SDLWindow.h"
#include "Tbx/Memory/Refs.h"
#include <SDL3/SDL_init.h>

namespace Tbx::Plugins::SDLWindowing
{
    SDLWindowFactoryPlugin::SDLWindowFactoryPlugin(Ref<EventBus> eventBus)
        : _listener(eventBus)
        , _usingOpenGl(false)
    {
        TBX_ASSERT(SDL_Init(SDL_INIT_VIDEO) != 0, "Failed to initialize SDL");
        _listener.Listen<AppSettingsChangedEvent>([this](const AppSettingsChangedEvent& e) { OnAppSettingsChanged(e); });
    }

    SDLWindowFactoryPlugin::~SDLWindowFactoryPlugin()
    {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);

        // Allow whichever plugin shuts down last to clean up SDL globally.
        if (SDL_WasInit(0) == 0)
        {
            SDL_Quit();
        }
    }

    std::shared_ptr<Window> SDLWindowFactoryPlugin::Create(const std::string& title, const Size& size, const WindowMode& mode, Ref<EventBus> eventBus)
    {
        auto window = MakeRef<SDLWindow>(_usingOpenGl, eventBus);
        window->SetTitle(title);
        window->SetSize(size);
        window->SetMode(mode);
        return window;
    }

    void SDLWindowFactoryPlugin::OnAppSettingsChanged(const AppSettingsChangedEvent& e)
    {
        _usingOpenGl = e.NewSettings.RenderingApi == GraphicsApi::OpenGL;
    }
}
