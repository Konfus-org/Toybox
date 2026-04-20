#include "tbx/plugins/sdl_opengl_adapter/sdl_opengl_adapter_plugin.h"
#include "tbx/app/settings.h"
#include "tbx/debugging/macros.h"
#include <string>
#include <vector>

namespace sdl_opengl_adapter
{
    static SdlOpenGlAdapterSettings get_default_open_gl_settings(bool vsync_enabled)
    {
        SdlOpenGlAdapterSettings settings = {};
        settings.is_vsync_enabled = vsync_enabled;
#if defined(TBX_DEBUG)
        settings.is_debug_context_enabled = true;
#endif
        return settings;
    }

    void SdlOpenGlAdapterPlugin::on_attach(tbx::ServiceProvider& service_provider)
    {
        auto& settings = service_provider.get_service<tbx::AppSettings>();
        _window_manager = service_provider.try_get_service<tbx::IWindowManager>();
        _use_opengl = settings.graphics.graphics_api == tbx::GraphicsApi::OPEN_GL;
        _vsync_enabled = settings.graphics.vsync_enabled;

        if (_window_manager)
        {
            for (const auto& window_id : _window_manager->get_open_windows())
            {
                auto* native_window =
                    static_cast<SDL_Window*>(_window_manager->get_native_handle(window_id));
                if (!native_window)
                    continue;

                _native_windows[window_id] = native_window;
                _window_sizes[window_id] = _window_manager->get_size(window_id);
            }
        }

        ensure_open_gl_adapter();
    }

    void SdlOpenGlAdapterPlugin::on_detach()
    {
        _open_gl_adapter.reset();
        _window_sizes.clear();
        _native_windows.clear();
        _window_manager = nullptr;
    }

    void SdlOpenGlAdapterPlugin::on_recieve_message(tbx::Message& msg)
    {
        if (auto* native_handle_event = handle_message<tbx::WindowNativeHandleChangedEvent>(msg))
        {
            on_window_native_handle_changed(*native_handle_event);
            return;
        }

        if (auto* make_current = handle_message<tbx::WindowMakeCurrentRequest>(msg))
        {
            handle_make_current(*make_current);
            return;
        }

        if (auto* present_request = handle_message<tbx::WindowPresentRequest>(msg))
        {
            handle_present(*present_request);
            return;
        }

        if (auto* vsync_event = handle_property_changed<&tbx::GraphicsSettings::vsync_enabled>(msg))
        {
            _vsync_enabled = vsync_event->current;
            apply_vsync_setting();
            return;
        }

        if (auto* size_event = handle_message<tbx::WindowSizeChangedEvent>(msg))
        {
            on_window_size_changed(*size_event);
            return;
        }

        if (const auto* graphics_event =
                handle_property_changed<&tbx::GraphicsSettings::graphics_api>(msg))
        {
            _use_opengl = graphics_event->current == tbx::GraphicsApi::OPEN_GL;
            ensure_open_gl_adapter();

            if (_use_opengl && _open_gl_adapter)
            {
                for (const auto& [window_id, native_window] : _native_windows)
                {
                    tbx::Size size = {};
                    if (_window_sizes.contains(window_id))
                        size = _window_sizes[window_id];
                    std::string label = to_string(window_id);

                    if (!_open_gl_adapter->try_create_context(native_window, label))
                        continue;

                    send_message<tbx::WindowContextReadyEvent>(
                        window_id,
                        _open_gl_adapter->get_proc_address(),
                        size);
                }
            }
            return;
        }
    }

    void SdlOpenGlAdapterPlugin::ensure_open_gl_adapter()
    {
        if (!_use_opengl)
        {
            _open_gl_adapter.reset();
            return;
        }

        if (!_open_gl_adapter)
            _open_gl_adapter =
                std::make_unique<SdlOpenGlAdapter>(get_default_open_gl_settings(_vsync_enabled));

        _open_gl_adapter->set_settings(get_default_open_gl_settings(_vsync_enabled));
        _open_gl_adapter->apply_default_attributes();
    }

    void SdlOpenGlAdapterPlugin::on_window_native_handle_changed(
        const tbx::WindowNativeHandleChangedEvent& event)
    {
        auto* previous_window = static_cast<SDL_Window*>(event.previous);
        if (!previous_window && _native_windows.contains(event.window))
            previous_window = _native_windows[event.window];

        auto* current_window = static_cast<SDL_Window*>(event.current);
        if (!current_window)
        {
            if (_open_gl_adapter && previous_window)
                _open_gl_adapter->destroy_context(previous_window);

            _window_sizes.erase(event.window);
            _native_windows.erase(event.window);
            return;
        }

        if (_open_gl_adapter && previous_window && previous_window != current_window)
            _open_gl_adapter->destroy_context(previous_window);

        _native_windows[event.window] = current_window;
        if (_window_manager)
            _window_sizes[event.window] = _window_manager->get_size(event.window);

        if (!_use_opengl)
            return;

        ensure_open_gl_adapter();
        if (!_open_gl_adapter)
            return;

        std::string label = to_string(event.window);
        if (!_open_gl_adapter->try_create_context(current_window, label))
            return;

        send_message<tbx::WindowContextReadyEvent>(
            event.window,
            _open_gl_adapter->get_proc_address(),
            _window_sizes[event.window]);
    }

    void SdlOpenGlAdapterPlugin::on_window_size_changed(const tbx::WindowSizeChangedEvent& event)
    {
        _window_sizes[event.window] = event.current;
    }

    void SdlOpenGlAdapterPlugin::handle_make_current(tbx::WindowMakeCurrentRequest& request)
    {
        if (!_use_opengl || !_open_gl_adapter)
        {
            request.state = tbx::MessageState::ERROR;
            request.result.flag_failure("SDL OpenGL adapter: OpenGL is not active.");
            return;
        }

        if (!_native_windows.contains(request.window))
        {
            request.state = tbx::MessageState::ERROR;
            request.result.flag_failure("SDL OpenGL adapter: native window not available.");
            return;
        }

        SDL_Window* native_window = _native_windows[request.window];
        std::string label = to_string(request.window);
        if (!_open_gl_adapter->try_make_current(native_window, label))
        {
            request.state = tbx::MessageState::ERROR;
            request.result.flag_failure(SDL_GetError());
            return;
        }

        request.state = tbx::MessageState::HANDLED;
        request.result.flag_success();
    }

    void SdlOpenGlAdapterPlugin::handle_present(tbx::WindowPresentRequest& request)
    {
        if (!_use_opengl || !_open_gl_adapter)
        {
            request.state = tbx::MessageState::ERROR;
            request.result.flag_failure("SDL OpenGL adapter: OpenGL is not active.");
            return;
        }

        if (!_native_windows.contains(request.window))
        {
            request.state = tbx::MessageState::ERROR;
            request.result.flag_failure("SDL OpenGL adapter: native window not available.");
            return;
        }

        SDL_Window* native_window = _native_windows[request.window];
        if (!_open_gl_adapter->try_present(native_window))
        {
            request.state = tbx::MessageState::ERROR;
            request.result.flag_failure("SDL OpenGL adapter: present failed.");
            return;
        }

        request.state = tbx::MessageState::HANDLED;
        request.result.flag_success();
    }

    void SdlOpenGlAdapterPlugin::apply_vsync_setting()
    {
        if (!_use_opengl || !_open_gl_adapter)
            return;

        _open_gl_adapter->set_settings(get_default_open_gl_settings(_vsync_enabled));

        std::vector<SDL_Window*> windows = {};
        windows.reserve(_native_windows.size());
        for (const auto& entry : _native_windows)
        {
            windows.push_back(entry.second);
        }

        _open_gl_adapter->apply_vsync_setting(windows);
    }
}
