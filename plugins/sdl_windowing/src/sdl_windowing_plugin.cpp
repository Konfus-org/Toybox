#include "sdl_windowing_plugin.h"
#include "tbx/app/application.h"
#include "tbx/debugging/macros.h"
#include "tbx/messages/observable.h"
#include <algorithm>

namespace tbx::plugins
{
    static void set_opengl_attribute(SDL_GLAttr attribute, int value)
    {
        if (!SDL_GL_SetAttribute(attribute, value))
        {
            TBX_TRACE_WARNING(
                "Failed to set SDL OpenGL attribute {}: {}",
                static_cast<int>(attribute),
                SDL_GetError());
        }
    }

    static WindowMode get_window_mode_from_flags(SDL_Window* sdl_window, WindowMode fallback_mode)
    {
        if (!sdl_window)
        {
            return fallback_mode;
        }

        const Uint32 flags = SDL_GetWindowFlags(sdl_window);
        if ((flags & SDL_WINDOW_FULLSCREEN) != 0)
        {
            return WindowMode::Fullscreen;
        }
        if ((flags & SDL_WINDOW_MINIMIZED) != 0)
        {
            return WindowMode::Minimized;
        }
        if ((flags & SDL_WINDOW_BORDERLESS) != 0)
        {
            return WindowMode::Borderless;
        }
        return WindowMode::Windowed;
    }

    static SDL_Window* create_sdl_window(Window* tbx_window, bool use_opengl)
    {
        uint flags = use_opengl ? SDL_WINDOW_OPENGL : 0;
        if (tbx_window->mode == WindowMode::Borderless)
            flags |= SDL_WINDOW_BORDERLESS;
        else if (tbx_window->mode == WindowMode::Fullscreen)
            flags |= SDL_WINDOW_FULLSCREEN;
        else if (tbx_window->mode == WindowMode::Windowed)
            flags |= SDL_WINDOW_RESIZABLE;
        else if (tbx_window->mode == WindowMode::Minimized)
            flags |= SDL_WINDOW_MINIMIZED;

        std::string& title = tbx_window->title;
        Size& size = tbx_window->size;
        SDL_Window* native = SDL_CreateWindow(title.c_str(), size.width, size.height, flags);
        if (!native)
        {
            TBX_ASSERT(false, "Failed to create SDL window!");
            return nullptr;
        }

        return native;
    }

    void SdlWindowingPlugin::on_attach(Application& host)
    {
        if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
        {
            TBX_TRACE_ERROR("Failed to initialize SDL video subsystem. See SDL logs for details.");
            return;
        }
        else
            TBX_TRACE_INFO("Initialized SDL video subsystem.");

        _use_opengl = host.get_settings().graphics_api == GraphicsApi::OpenGL;
        if (_use_opengl)
        {
            set_opengl_attribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
            set_opengl_attribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
            set_opengl_attribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
            set_opengl_attribute(SDL_GL_DEPTH_SIZE, 24);
            set_opengl_attribute(SDL_GL_STENCIL_SIZE, 8);
            set_opengl_attribute(SDL_GL_DOUBLEBUFFER, 1);
#if defined(TBX_DEBUG)
            set_opengl_attribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif
        }
    }

    void SdlWindowingPlugin::on_detach()
    {
        for (auto& record : _windows)
        {
            destroy_gl_context(record);
            SDL_DestroyWindow(record.sdl_window);
        }
        _windows.clear();
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }

    void SdlWindowingPlugin::on_update(const DeltaTime&)
    {
        SDL_Event event;
        if (!SDL_PollEvent(&event))
            return;

        switch (event.type)
        {
            case SDL_EventType::SDL_EVENT_WINDOW_FOCUS_GAINED:
            {
                if (_use_opengl)
                {
                    auto* window = SDL_GetWindowFromID(event.window.windowID);
                    SdlWindowRecord* record = try_get_record(window);
                    if (record && record->sdl_window && record->gl_context)
                    {
                        if (SDL_GL_MakeCurrent(record->sdl_window, record->gl_context))
                            TBX_TRACE_INFO(
                                "SDL windowing: Made OpenGL context current for window '{}'.",
                                record->tbx_window ? record->tbx_window->title.value : "<unknown>");
                        else
                            TBX_TRACE_ERROR(
                                "SDL windowing: Failed to make OpenGL context current for window "
                                "'{}': {}",
                                record->tbx_window ? record->tbx_window->title.value : "<unknown>",
                                SDL_GetError());
                    }
                }
                break;
            }
            case SDL_EventType::SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            {
                auto* window = SDL_GetWindowFromID(event.window.windowID);
                SdlWindowRecord* record = try_get_record(window);
                if (record && record->tbx_window)
                {
                    record->tbx_window->is_open = false;
                }
                break;
            }
            case SDL_EventType::SDL_EVENT_WINDOW_RESIZED:
            {
                auto* window = SDL_GetWindowFromID(event.window.windowID);
                SdlWindowRecord* record = try_get_record(window);
                if (record && record->tbx_window)
                {
                    auto new_size = Size(
                        static_cast<uint>(event.window.data1),
                        static_cast<uint>(event.window.data2));
                    record->tbx_window->size = new_size;
                }
                break;
            }
            case SDL_EventType::SDL_EVENT_WINDOW_MINIMIZED:
            {
                auto* window = SDL_GetWindowFromID(event.window.windowID);
                SdlWindowRecord* record = try_get_record(window);
                if (record && record->tbx_window)
                {
                    if (record->tbx_window->mode != WindowMode::Minimized)
                    {
                        record->last_window_mode = record->tbx_window->mode;
                    }
                    record->tbx_window->mode = WindowMode::Minimized;
                }
                break;
            }
            case SDL_EventType::SDL_EVENT_WINDOW_RESTORED:
            {
                auto* window = SDL_GetWindowFromID(event.window.windowID);
                SdlWindowRecord* record = try_get_record(window);
                if (record && record->tbx_window)
                {
                    WindowMode target_mode = record->last_window_mode;
                    if (target_mode == WindowMode::Minimized)
                    {
                        target_mode =
                            get_window_mode_from_flags(record->sdl_window, WindowMode::Windowed);
                    }
                    if (target_mode == WindowMode::Minimized)
                    {
                        target_mode = WindowMode::Windowed;
                    }
                    record->last_window_mode = record->tbx_window->mode;
                    record->tbx_window->mode = target_mode;
                }
                break;
            }
            case SDL_EventType::SDL_EVENT_WINDOW_MAXIMIZED:
            {
                auto* window = SDL_GetWindowFromID(event.window.windowID);
                SdlWindowRecord* record = try_get_record(window);
                if (record && record->tbx_window)
                {
                    WindowMode target_mode =
                        get_window_mode_from_flags(record->sdl_window, record->last_window_mode);
                    if (target_mode == WindowMode::Minimized)
                    {
                        target_mode = WindowMode::Windowed;
                    }
                    record->last_window_mode = record->tbx_window->mode;
                    record->tbx_window->mode = target_mode;
                }
                break;
            }
            case SDL_EventType::SDL_EVENT_QUIT:
            {
                for (auto& record : _windows)
                {
                    destroy_gl_context(record);
                    SDL_DestroyWindow(record.sdl_window);
                }
                _windows.clear();
                break;
            }
            default:
                break;
        }
    }

    void SdlWindowingPlugin::on_recieve_message(Message& msg)
    {
        if (auto* make_current = handle_message<WindowMakeCurrentRequest>(msg))
        {
            on_window_make_current(*make_current);
            return;
        }

        if (auto* present_request = handle_message<WindowPresentRequest>(msg))
        {
            on_window_present(*present_request);
            return;
        }

        // Graphics api changed
        if (auto* graphics_event = handle_property_changed<&AppSettings::graphics_api>(msg))
        {
            _use_opengl = graphics_event->current == GraphicsApi::OpenGL;

            // Recreate all windows with new graphics api
            for (auto& record : _windows)
            {
                // Destory old SDL window
                destroy_gl_context(record);
                SDL_DestroyWindow(record.sdl_window);
                record.sdl_window = nullptr;
                record.gl_context = nullptr;

                // Create new SDL window
                Window* window = record.tbx_window;
                SDL_Window* native = create_sdl_window(window, _use_opengl);
                record.sdl_window = native;
                if (_use_opengl)
                    record.gl_context = create_gl_context(native, window);
            }
            return;
        }

        // Window closed
        if (auto* open_event = handle_property_changed<&Window::is_open>(msg))
        {
            on_window_is_open_changed(*open_event);
            return;
        }

        // Window resized
        if (auto* size_event = handle_property_changed<&Window::size>(msg))
        {
            on_window_size_changed(*size_event);
            return;
        }

        // Window mode changed
        if (auto* mode_event = handle_property_changed<&Window::mode>(msg))
        {
            on_window_mode_changed(*mode_event);
            return;
        }

        // Window name changed
        if (auto* title_event = handle_property_changed<&Window::title>(msg))
        {
            on_window_title_changed(*title_event);
            return;
        }
    }

    void SdlWindowingPlugin::on_window_is_open_changed(PropertyChangedEvent<Window, bool>& event)
    {
        // Window is closing
        if (event.current == false)
        {
            SdlWindowRecord* record = try_get_record(event.owner);
            if (!record)
            {
                return;
            }
            destroy_gl_context(*record);
            SDL_DestroyWindow(record->sdl_window);
            remove_record(*record);
        }
        // Window is opening
        else
        {
            Window* window = event.owner;
            SDL_Window* native = create_sdl_window(window, _use_opengl);
            SdlWindowRecord& record = add_record(native, window);
            if (_use_opengl)
            {
                record.gl_context = create_gl_context(native, window);
            }
        }
    }

    void SdlWindowingPlugin::on_window_size_changed(PropertyChangedEvent<Window, Size>& event)
    {
        SdlWindowRecord* record = try_get_record(event.owner);
        if (record && record->sdl_window)
        {
            SDL_SetWindowSize(
                record->sdl_window,
                static_cast<int>(event.current.width),
                static_cast<int>(event.current.height));
        }
    }

    void SdlWindowingPlugin::on_window_mode_changed(PropertyChangedEvent<Window, WindowMode>& event)
    {
        SdlWindowRecord* record = try_get_record(event.owner);
        if (!record || !record->sdl_window)
        {
            return;
        }

        record->last_window_mode = event.previous;
        if (event.current == WindowMode::Borderless)
            SDL_SetWindowBordered(record->sdl_window, false);
        else if (event.current == WindowMode::Fullscreen)
            SDL_SetWindowFullscreen(record->sdl_window, true);
        else if (event.current == WindowMode::Minimized)
            SDL_MinimizeWindow(record->sdl_window);
        else if (event.current == WindowMode::Windowed)
        {
            SDL_SetWindowBordered(record->sdl_window, true);
            SDL_SetWindowFullscreen(record->sdl_window, false);
            SDL_RestoreWindow(record->sdl_window);
        }
        else
            TBX_ASSERT(false, "Unhandled window mode change!");
    }

    void SdlWindowingPlugin::on_window_title_changed(
        PropertyChangedEvent<Window, std::string>& event)
    {
        SdlWindowRecord* record = try_get_record(event.owner);
        if (record && record->sdl_window)
        {
            SDL_SetWindowTitle(record->sdl_window, event.current.c_str());
        }
    }

    void SdlWindowingPlugin::on_window_make_current(WindowMakeCurrentRequest& request)
    {
        SdlWindowRecord* record = try_get_record(request.window);
        if (!record || !record->sdl_window || !record->gl_context)
        {
            request.state = MessageState::Error;
            request.result.flag_failure("SDL windowing: OpenGL context not available.");
            return;
        }

        if (_use_opengl && !SDL_GL_MakeCurrent(record->sdl_window, record->gl_context))
        {
            request.state = MessageState::Error;
            request.result.flag_failure(SDL_GetError());
            return;
        }

        request.state = MessageState::Handled;
        request.result.flag_success();
    }

    void SdlWindowingPlugin::on_window_present(WindowPresentRequest& request)
    {
        SdlWindowRecord* record = try_get_record(request.window);
        if (!record || !record->sdl_window || !record->gl_context)
        {
            request.state = MessageState::Error;
            request.result.flag_failure("SDL windowing: OpenGL context not available.");
            return;
        }

        SDL_GL_SwapWindow(record->sdl_window);
        request.state = MessageState::Handled;
        request.result.flag_success();
    }

    SDL_GLContext SdlWindowingPlugin::create_gl_context(SDL_Window* sdl_window, Window* tbx_window)
    {
        if (!sdl_window || !tbx_window || !_use_opengl)
        {
            return nullptr;
        }

        SDL_GLContext context = SDL_GL_CreateContext(sdl_window);
        if (!context)
        {
            TBX_TRACE_ERROR(
                "Failed to create SDL OpenGL context for window '{}': {}",
                tbx_window->title.value,
                SDL_GetError());
            return nullptr;
        }

        if (!SDL_GL_MakeCurrent(sdl_window, context))
        {
            TBX_TRACE_ERROR(
                "Failed to make SDL OpenGL context current for window '{}': {}",
                tbx_window->title.value,
                SDL_GetError());
        }

        const auto get_proc_address = reinterpret_cast<GraphicsProcAddress>(SDL_GL_GetProcAddress);
        send_message<WindowContextReadyEvent>(
            tbx_window->id,
            get_proc_address,
            tbx_window->size.value);
        return context;
    }

    void SdlWindowingPlugin::destroy_gl_context(SdlWindowRecord& record)
    {
        if (record.gl_context)
        {
            SDL_GL_DestroyContext(record.gl_context);
            record.gl_context = nullptr;
        }
    }

    SdlWindowRecord* SdlWindowingPlugin::try_get_record(
        std::function<bool(const SdlWindowRecord&)> condition)
    {
        auto it = std::find_if(
            _windows.begin(),
            _windows.end(),
            [&condition](const SdlWindowRecord& record)
            {
                return condition(record);
            });
        if (it != _windows.end())
        {
            return &(*it);
        }
        return nullptr;
    }

    SdlWindowRecord* SdlWindowingPlugin::try_get_record(const SDL_Window* sdl_window)
    {
        return try_get_record(
            [&sdl_window](const SdlWindowRecord& record)
            {
                return record.sdl_window == sdl_window;
            });
    }

    SdlWindowRecord* SdlWindowingPlugin::try_get_record(const Window* tbx_window)
    {
        return try_get_record(
            [&tbx_window](const SdlWindowRecord& record)
            {
                return record.tbx_window == tbx_window;
            });
    }

    SdlWindowRecord* SdlWindowingPlugin::try_get_record(const Uuid& window_id)
    {
        return try_get_record(
            [&window_id](const SdlWindowRecord& record)
            {
                return record.tbx_window && record.tbx_window->id == window_id;
            });
    }

    SdlWindowRecord& SdlWindowingPlugin::add_record(SDL_Window* sdl_window, Window* tbx_window)
    {
        _windows.emplace_back();
        _windows.back().sdl_window = sdl_window;
        _windows.back().tbx_window = tbx_window;
        if (tbx_window)
        {
            _windows.back().last_window_mode = tbx_window->mode;
        }
        return _windows.back();
    }

    void SdlWindowingPlugin::remove_record(const SdlWindowRecord& record)
    {
        _windows.erase(
            std::ranges::remove_if(
                _windows,
                [&record](const SdlWindowRecord& rec)
                {
                    return rec.sdl_window == record.sdl_window;
                })
                .begin(),
            _windows.end());
    }
}
