#include "sdl_windowing_plugin.h"
#include "tbx/systems/debugging/macros.h"
#include <SDL3/SDL.h>

namespace sdl_windowing
{
    void SdlWindowingPlugin::on_attach()
    {
        if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
        {
            TBX_TRACE_ERROR("Failed to initialize SDL video subsystem. Error: {}", SDL_GetError());
            return;
        }

        TBX_TRACE_INFO("Initialized SDL video subsystem.");
        TBX_TRACE_INFO("Video driver: {}", SDL_GetCurrentVideoDriver());
    }

    void SdlWindowingPlugin::on_detach()
    {
        if (window_backend)
            window_backend->shutdown();
        window_backend = {};

        if (SDL_WasInit(SDL_INIT_VIDEO) != 0)
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }
}
