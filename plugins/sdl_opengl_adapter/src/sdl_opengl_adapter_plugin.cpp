#include "tbx/plugins/sdl_opengl_adapter/sdl_opengl_adapter_plugin.h"
#include "tbx/app/settings.h"
#include "tbx/debugging/macros.h"
#include <memory>

namespace sdl_opengl_adapter
{
    namespace
    {
        SdlOpenGlAdapterSettings get_default_open_gl_settings()
        {
            auto settings = SdlOpenGlAdapterSettings {};
#if defined(TBX_DEBUG)
            settings.is_debug_context_enabled = true;
#endif
            return settings;
        }

        tbx::VsyncMode to_vsync_mode(const bool enabled)
        {
            return enabled ? tbx::VsyncMode::ON : tbx::VsyncMode::OFF;
        }
    }

    void SdlOpenGlAdapterPlugin::on_attach(tbx::ServiceProvider& service_provider)
    {
        _service_provider = &service_provider;
        _window_manager = service_provider.try_get_service<tbx::IWindowManager>();
        auto& settings = service_provider.get_service<tbx::AppSettings>();
        _use_opengl = settings.graphics.graphics_api == tbx::GraphicsApi::OPEN_GL;
        _vsync_enabled = settings.graphics.vsync_enabled;

        TBX_ASSERT(_window_manager != nullptr, "SDL OpenGL adapter requires an IWindowManager.");
        if (!_window_manager)
            return;

        service_provider.register_service<tbx::IGraphicsContextManager>(
            std::make_unique<SdlOpenGlAdapter>(*_window_manager, get_default_open_gl_settings()));
        _open_gl_adapter = static_cast<SdlOpenGlAdapter*>(
            service_provider.try_get_service<tbx::IGraphicsContextManager>());
        configure_adapter();
    }

    void SdlOpenGlAdapterPlugin::on_detach()
    {
        if (_service_provider && _service_provider->has_service<tbx::IGraphicsContextManager>())
            _service_provider->deregister_service<tbx::IGraphicsContextManager>();

        _open_gl_adapter = nullptr;
        _window_manager = nullptr;
        _service_provider = nullptr;
        _use_opengl = false;
        _vsync_enabled = false;
    }

    void SdlOpenGlAdapterPlugin::on_recieve_message(tbx::Message& msg)
    {
        if (!_open_gl_adapter)
            return;

        if (const auto* native_handle_event =
                tbx::handle_message<tbx::WindowNativeHandleChangedEvent>(msg))
        {
            _open_gl_adapter->sync_window(
                native_handle_event->window,
                static_cast<SDL_Window*>(native_handle_event->current));
            return;
        }

        if (const auto* vsync_event =
                tbx::handle_property_changed<&tbx::GraphicsSettings::vsync_enabled>(msg))
        {
            _vsync_enabled = vsync_event->current;
            _open_gl_adapter->set_vsync(to_vsync_mode(_vsync_enabled));
            return;
        }

        if (const auto* graphics_event =
                tbx::handle_property_changed<&tbx::GraphicsSettings::graphics_api>(msg))
        {
            _use_opengl = graphics_event->current == tbx::GraphicsApi::OPEN_GL;
            configure_adapter();
        }
    }

    void SdlOpenGlAdapterPlugin::configure_adapter()
    {
        if (!_open_gl_adapter)
            return;

        _open_gl_adapter->set_settings(get_default_open_gl_settings());
        if (!_use_opengl)
            return;

        _open_gl_adapter->apply_default_attributes();
        _open_gl_adapter->set_vsync(to_vsync_mode(_vsync_enabled));
        sync_open_windows();
    }

    void SdlOpenGlAdapterPlugin::sync_open_windows() const
    {
        if (!_open_gl_adapter || !_window_manager)
            return;

        for (const auto& window : _window_manager->get_open_windows())
        {
            _open_gl_adapter->sync_window(
                window,
                static_cast<SDL_Window*>(_window_manager->get_native_handle(window)));
        }
    }
}
