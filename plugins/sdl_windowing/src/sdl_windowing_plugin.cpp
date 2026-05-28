#include "tbx/plugins/sdl_windowing/sdl_windowing_plugin.h"
#include "sdl_window_backend.h"
#include "tbx/systems/app/settings.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/messaging/observable.h"
#include "tbx/systems/windowing/manager.h"
#include <filesystem>
#include <memory>
#include <string_view>

namespace sdl_windowing
{
    static bool is_wayland_video_driver_for_plugin()
    {
        const char* video_driver = SDL_GetCurrentVideoDriver();
        return video_driver != nullptr && std::string_view(video_driver) == "wayland";
    }

    static SDL_Surface* try_load_icon_surface(const std::filesystem::path& icon_path)
    {
        if (icon_path.empty())
            return nullptr;

        if (is_wayland_video_driver_for_plugin())
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

    void SdlWindowingPlugin::on_attach(tbx::ServiceProvider& service_provider)
    {
        if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
        {
            TBX_TRACE_ERROR("Failed to initialize SDL video subsystem. Error: {}", SDL_GetError());
            return;
        }

        TBX_TRACE_INFO("Initialized SDL video subsystem.");
        TBX_TRACE_INFO("Video driver: {}", SDL_GetCurrentVideoDriver());
        auto settings = service_provider.get_service<tbx::AppSettings>().lock();
        TBX_ASSERT(settings != nullptr, "SDL windowing plugin requires AppSettings service.");
        if (!settings)
            return;

        _use_opengl = settings->graphics->graphics_api == tbx::GraphicsApi::OPEN_GL;

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
        window_backend->set_use_opengl(_use_opengl);

        if (!service_provider.has_service<tbx::IWindowManager>())
        {
            auto msg_coordinator = service_provider.get_service<tbx::IMessageCoordinator>().lock();
            TBX_ASSERT(
                msg_coordinator != nullptr,
                "SDL windowing plugin requires IMessageCoordinator service.");
            if (!msg_coordinator)
                return;

            service_provider.register_service<tbx::IWindowManager>(
                std::make_unique<tbx::WindowManager>(
                    service_provider.get_service<tbx::IMessageCoordinator>(),
                    _window_backend));
        }

        auto asset_manager = service_provider.get_service<tbx::AssetManager>().lock();
        TBX_ASSERT(asset_manager != nullptr, "SDL windowing plugin requires AssetManager service.");
        if (!asset_manager)
            return;

        const auto& const_asset_manager = static_cast<const tbx::AssetManager&>(*asset_manager);
        const std::filesystem::path icon_path = const_asset_manager.resolve(settings->icon);
        if (icon_path.empty())
        {
            TBX_TRACE_WARNING(
                "Failed to resolve app icon handle to a path. Window icon will not be set.");
            return;
        }

        _window_icon_surface = try_load_icon_surface(icon_path);
        if (auto window_backend_ptr = _window_backend.lock())
            window_backend_ptr->set_icon_surface(_window_icon_surface);
    }

    void SdlWindowingPlugin::on_detach(tbx::ServiceProvider& service_provider)
    {
        if (service_provider.has_service<tbx::IWindowManager>())
            service_provider.deregister_service<tbx::IWindowManager>();

        if (service_provider.has_service<tbx::IWindowBackend>())
            service_provider.deregister_service<tbx::IWindowBackend>();

        _window_backend = {};

        if (_window_icon_surface)
        {
            SDL_DestroySurface(_window_icon_surface);
            _window_icon_surface = nullptr;
        }

        if (SDL_WasInit(SDL_INIT_VIDEO) != 0)
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }

    void SdlWindowingPlugin::on_update(const tbx::DeltaTime&) {}

    void SdlWindowingPlugin::on_recieve_message(tbx::Message& msg)
    {
        auto window_backend = _window_backend.lock();
        if (!window_backend)
            return;

        if (const auto graphics_event =
                tbx::handle_property_changed<&tbx::GraphicsSettings::graphics_api>(msg))
        {
            _use_opengl = graphics_event->get().current == tbx::GraphicsApi::OPEN_GL;
            window_backend->set_use_opengl(_use_opengl);
        }
    }
}
