#include "tbx/plugins/sdl_windowing/sdl_windowing_plugin.h"
#include "sdl_window_manager.h"
#include "tbx/systems/app/settings.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/messaging/observable.h"
#include "tbx/types/handle.h"
#include <filesystem>
#include <string_view>


namespace sdl_windowing
{
    namespace
    {
        bool is_wayland_video_driver()
        {
            const char* video_driver = SDL_GetCurrentVideoDriver();
            return video_driver != nullptr && std::string_view(video_driver) == "wayland";
        }

        SDL_Surface* try_load_icon_surface(const std::filesystem::path& icon_path)
        {
            if (icon_path.empty())
                return nullptr;

            if (is_wayland_video_driver())
                return nullptr;

            SDL_ClearError();
            if (SDL_Surface* icon_surface = SDL_LoadSurface(icon_path.string().c_str()))
            {
                TBX_TRACE_INFO("Loaded app icon '{}'.", icon_path.string());
                return icon_surface;
            }

            TBX_TRACE_WARNING(
                "Failed to load app icon '{}'. Error: {}",
                icon_path.string(),
                SDL_GetError());
            SDL_ClearError();
            return nullptr;
        }
    }

    void SdlWindowingPlugin::on_attach(tbx::ServiceProvider& service_provider)
    {
        _service_provider = &service_provider;

        if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
        {
            TBX_TRACE_ERROR("Failed to initialize SDL video subsystem. Error: {}", SDL_GetError());
            return;
        }

        TBX_TRACE_INFO("Initialized SDL video subsystem.");
        TBX_TRACE_INFO("Video driver: {}", SDL_GetCurrentVideoDriver());
        _use_opengl = service_provider.get_service<tbx::AppSettings>().graphics.graphics_api
                      == tbx::GraphicsApi::OPEN_GL;

        service_provider.register_service<tbx::IWindowManager>(std::make_unique<SdlWindowManager>(
            service_provider.get_service<tbx::IMessageCoordinator>()));
        _window_manager =
            static_cast<SdlWindowManager*>(service_provider.try_get_service<tbx::IWindowManager>());
        if (_window_manager)
            _window_manager->set_use_opengl(_use_opengl);

        const tbx::AssetManager& asset_manager = service_provider.get_service<tbx::AssetManager>();
        const std::filesystem::path icon_path =
            asset_manager.resolve(service_provider.get_service<tbx::Handle>());
        if (icon_path.empty())
        {
            TBX_TRACE_WARNING(
                "Failed to resolve app icon handle to a path. Window icon will not be set.");
            return;
        }

        _window_icon_surface = try_load_icon_surface(icon_path);
        if (_window_manager)
            _window_manager->set_icon_surface(_window_icon_surface);
    }

    void SdlWindowingPlugin::on_detach()
    {
        if (_window_manager)
            _window_manager->shutdown();

        if (_service_provider && _service_provider->has_service<tbx::IWindowManager>())
            _service_provider->deregister_service<tbx::IWindowManager>();

        _window_manager = nullptr;
        _service_provider = nullptr;

        if (_window_icon_surface)
        {
            SDL_DestroySurface(_window_icon_surface);
            _window_icon_surface = nullptr;
        }

        if (SDL_WasInit(SDL_INIT_VIDEO) != 0)
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }

    void SdlWindowingPlugin::on_update(const tbx::DeltaTime&)
    {
        if (!_window_manager)
            return;

        SDL_Event event = {};
        while (SDL_PollEvent(&event))
            _window_manager->process_event(event);

        _window_manager->process_pending_window_closes();
    }

    void SdlWindowingPlugin::on_recieve_message(tbx::Message& msg)
    {
        if (!_window_manager)
            return;

        if (const auto* graphics_event =
                tbx::handle_property_changed<&tbx::GraphicsSettings::graphics_api>(msg))
        {
            _use_opengl = graphics_event->current == tbx::GraphicsApi::OPEN_GL;
            _window_manager->set_use_opengl(_use_opengl);
        }
    }
}
