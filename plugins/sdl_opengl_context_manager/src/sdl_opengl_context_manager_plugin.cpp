#include "tbx/plugins/sdl_opengl_context_manager/sdl_opengl_context_manager_plugin.h"
#include "tbx/app/settings.h"
#include <SDL3/SDL.h>
#include <memory>

namespace sdl_opengl_context_manager
{
    void SdlOpenGlContextManagerPlugin::on_attach(tbx::ServiceProvider& service_provider)
    {
        _service_provider = &service_provider;
        _settings = &service_provider.get_service<tbx::AppSettings>();
        _window_manager = service_provider.try_get_service<tbx::IWindowManager>();

        auto context_manager = std::make_unique<SdlOpenGlContextManager>();
        _context_manager = context_manager.get();
        service_provider.register_service<tbx::IOpenGlContextManager>(std::move(context_manager));

        initialize_context_manager();

        if (!_window_manager)
            return;

        for (const auto& window_id : _window_manager->get_open_windows())
            on_window_opened(tbx::WindowOpenedEvent(window_id));
    }

    void SdlOpenGlContextManagerPlugin::on_detach()
    {
        if (_service_provider)
            _service_provider->deregister_service<tbx::IOpenGlContextManager>();

        _context_manager = nullptr;
        _service_provider = nullptr;
        _settings = nullptr;
        _window_manager = nullptr;
    }

    void SdlOpenGlContextManagerPlugin::on_recieve_message(tbx::Message& msg)
    {
        if (const auto* opened_event = handle_message<tbx::WindowOpenedEvent>(msg))
        {
            on_window_opened(*opened_event);
            return;
        }

        if (const auto* native_handle_event =
                handle_message<tbx::WindowNativeHandleChangedEvent>(msg))
        {
            on_window_native_handle_changed(*native_handle_event);
            return;
        }

        if (const auto* closed_event = handle_message<tbx::WindowClosedEvent>(msg))
        {
            if (_context_manager)
                _context_manager->remove_window(closed_event->window);
            return;
        }

        if (handle_property_changed<&tbx::GraphicsSettings::graphics_api>(msg))
        {
            initialize_context_manager();
            return;
        }

        if (const auto* vsync_event =
                handle_property_changed<&tbx::GraphicsSettings::vsync_enabled>(msg))
        {
            if (_context_manager)
                _context_manager->set_vsync(
                    vsync_event->current ? tbx::VsyncMode::ON : tbx::VsyncMode::OFF);
            return;
        }
    }

    void SdlOpenGlContextManagerPlugin::initialize_context_manager()
    {
        if (!_context_manager || !_settings)
            return;

        bool debug_context_enabled = false;
#if defined(TBX_DEBUG)
        debug_context_enabled = true;
#endif

        _context_manager->initialize(
            4,
            5,
            24,
            8,
            true,
            debug_context_enabled,
            _settings->graphics.vsync_enabled);
    }

    void SdlOpenGlContextManagerPlugin::on_window_native_handle_changed(
        const tbx::WindowNativeHandleChangedEvent& event)
    {
        if (!_context_manager)
            return;

        auto* previous_window = static_cast<SDL_Window*>(event.previous);
        auto* current_window = static_cast<SDL_Window*>(event.current);
        if (!current_window)
        {
            _context_manager->remove_window(event.window, previous_window);
            return;
        }

        _context_manager->set_window(event.window, current_window);
    }

    void SdlOpenGlContextManagerPlugin::on_window_opened(const tbx::WindowOpenedEvent& event)
    {
        if (!_context_manager || !_window_manager)
            return;

        auto* native_window =
            static_cast<SDL_Window*>(_window_manager->get_native_handle(event.window));
        if (!native_window)
            return;

        _context_manager->set_window(event.window, native_window);
    }
}
