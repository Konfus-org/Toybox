#include "tbx/plugins/sdl_windowing/sdl_windowing_plugin.h"
#include "sdl_window_backend.h"
#include "tbx/systems/debugging/macros.h"
#include <SDL3/SDL.h>

namespace sdl_windowing
{
    void SdlWindowingPlugin::on_attach(tbx::ServiceProvider& service_provider)
    {
        if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
        {
            TBX_TRACE_ERROR("Failed to initialize SDL video subsystem. Error: {}", SDL_GetError());
            return;
        }

        TBX_TRACE_INFO("Initialized SDL video subsystem.");
        TBX_TRACE_INFO("Video driver: {}", SDL_GetCurrentVideoDriver());

        service_provider.register_service<tbx::IWindowBackend>(
            std::make_unique<SdlWindowBackend>());

        auto window_backend_service = service_provider.get_service<tbx::IWindowBackend>().lock();
        TBX_ASSERT(
            window_backend_service != nullptr,
            "SDL windowing plugin requires IWindowBackend service after registration.");
        if (!window_backend_service)
            return;

        auto window_backend = std::dynamic_pointer_cast<SdlWindowBackend>(window_backend_service);
        TBX_ASSERT(window_backend != nullptr, "SDL window backend service has unexpected type.");
        if (!window_backend)
            return;

        _window_backend = window_backend;
    }

    void SdlWindowingPlugin::on_detach(tbx::ServiceProvider& service_provider)
    {
        if (auto window_backend = _window_backend.lock())
            window_backend->shutdown();

        if (service_provider.has_service<tbx::IWindowBackend>())
            service_provider.deregister_service<tbx::IWindowBackend>();

        _window_backend = {};

        if (SDL_WasInit(SDL_INIT_VIDEO) != 0)
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }
}
