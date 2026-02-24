#include "sdl_opengl_adapter_plugin.h"
#include "tbx/app/application.h"
#include "tbx/debugging/macros.h"
#include "tbx/messages/observable.h"
#include <string>
#include <vector>

namespace tbx::plugins
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

    void SdlOpenGlAdapterPlugin::on_attach(IPluginHost& host)
    {
        _use_opengl = host.get_settings().graphics.graphics_api == GraphicsApi::OPEN_GL;
        _vsync_enabled = host.get_settings().graphics.vsync_enabled;
        ensure_open_gl_adapter();
    }

    void SdlOpenGlAdapterPlugin::on_detach()
    {
        _open_gl_adapter.reset();
        _window_sizes.clear();
        _native_windows.clear();
    }

    void SdlOpenGlAdapterPlugin::on_recieve_message(Message& msg)
    {
        if (auto* native_handle_event = handle_property_changed<&Window::native_handle>(msg))
        {
            on_window_native_handle_changed(*native_handle_event);
            return;
        }

        if (auto* make_current = handle_message<WindowMakeCurrentRequest>(msg))
        {
            handle_make_current(*make_current);
            return;
        }

        if (auto* present_request = handle_message<WindowPresentRequest>(msg))
        {
            handle_present(*present_request);
            return;
        }

        if (auto* vsync_event = handle_property_changed<&GraphicsSettings::vsync_enabled>(msg))
        {
            _vsync_enabled = vsync_event->current;
            apply_vsync_setting();
            return;
        }

        if (auto* size_event = handle_property_changed<&Window::size>(msg))
        {
            if (size_event->owner)
                _window_sizes[size_event->owner->id] = size_event->current;
            return;
        }

        if (auto* graphics_event = handle_property_changed<&GraphicsSettings::graphics_api>(msg))
        {
            _use_opengl = graphics_event->current == GraphicsApi::OPEN_GL;
            ensure_open_gl_adapter();

            if (_use_opengl && _open_gl_adapter)
            {
                for (const auto& entry : _native_windows)
                {
                    Uuid window_id = entry.first;
                    SDL_Window* native_window = entry.second;
                    Size size = {};
                    if (_window_sizes.contains(window_id))
                        size = _window_sizes[window_id];
                    std::string label = to_string(window_id);

                    if (!_open_gl_adapter->try_create_context(native_window, label))
                        continue;

                    send_message<WindowContextReadyEvent>(
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
        PropertyChangedEvent<Window, NativeWindowHandle>& event)
    {
        if (!event.owner)
            return;

        Uuid window_id = event.owner->id;
        _window_sizes[window_id] = event.owner->size.value;

        SDL_Window* current_window = reinterpret_cast<SDL_Window*>(event.current);
        if (!current_window)
        {
            SDL_Window* previous_window = nullptr;
            if (_native_windows.contains(window_id))
                previous_window = _native_windows[window_id];

            if (_open_gl_adapter && previous_window)
                _open_gl_adapter->destroy_context(previous_window);

            _window_sizes.erase(window_id);
            _native_windows.erase(window_id);
            return;
        }

        _native_windows[window_id] = current_window;

        if (!_use_opengl)
            return;

        ensure_open_gl_adapter();
        if (!_open_gl_adapter)
            return;

        std::string label = to_string(window_id);
        if (!_open_gl_adapter->try_create_context(current_window, label))
            return;

        send_message<WindowContextReadyEvent>(
            window_id,
            _open_gl_adapter->get_proc_address(),
            event.owner->size.value);
    }

    void SdlOpenGlAdapterPlugin::handle_make_current(WindowMakeCurrentRequest& request)
    {
        if (!_use_opengl || !_open_gl_adapter)
        {
            request.state = MessageState::ERROR;
            request.result.flag_failure("SDL OpenGL adapter: OpenGL is not active.");
            return;
        }

        if (!_native_windows.contains(request.window))
        {
            request.state = MessageState::ERROR;
            request.result.flag_failure("SDL OpenGL adapter: native window not available.");
            return;
        }

        SDL_Window* native_window = _native_windows[request.window];
        std::string label = to_string(request.window);
        if (!_open_gl_adapter->try_make_current(native_window, label))
        {
            request.state = MessageState::ERROR;
            request.result.flag_failure(SDL_GetError());
            return;
        }

        request.state = MessageState::HANDLED;
        request.result.flag_success();
    }

    void SdlOpenGlAdapterPlugin::handle_present(WindowPresentRequest& request)
    {
        if (!_use_opengl || !_open_gl_adapter)
        {
            request.state = MessageState::ERROR;
            request.result.flag_failure("SDL OpenGL adapter: OpenGL is not active.");
            return;
        }

        if (!_native_windows.contains(request.window))
        {
            request.state = MessageState::ERROR;
            request.result.flag_failure("SDL OpenGL adapter: native window not available.");
            return;
        }

        SDL_Window* native_window = _native_windows[request.window];
        if (!_open_gl_adapter->try_present(native_window))
        {
            request.state = MessageState::ERROR;
            request.result.flag_failure("SDL OpenGL adapter: present failed.");
            return;
        }

        request.state = MessageState::HANDLED;
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
