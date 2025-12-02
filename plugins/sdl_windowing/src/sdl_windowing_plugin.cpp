#include "sdl_windowing_plugin.h"
#include "tbx/app/application.h"
#include "tbx/app/window_events.h"
#include "tbx/debugging/macros.h"
#include "tbx/messages/handler.h"
#include <algorithm>
#include <string>
#include <utility>

namespace tbx::plugins::sdlwindowing
{
    // --------------------------------
    // SDL windowing helpers
    // --------------------------------

    static WindowMode resolve_window_mode(SDL_Window* window, WindowMode fallback)
    {
        auto flags = SDL_GetWindowFlags(window);
        if ((flags & SDL_WINDOW_MINIMIZED) == SDL_WINDOW_MINIMIZED)
        {
            return WindowMode::Minimized;
        }
        if ((flags & SDL_WINDOW_FULLSCREEN) == SDL_WINDOW_FULLSCREEN)
        {
            return WindowMode::Fullscreen;
        }
        if ((flags & SDL_WINDOW_BORDERLESS) == SDL_WINDOW_BORDERLESS)
        {
            return WindowMode::Borderless;
        }
        return fallback;
    }

    static WindowDescription read_window_description(SDL_Window* window)
    {
        WindowDescription description = {};

        int width = description.size.width;
        int height = description.size.height;
        SDL_GetWindowSize(window, &width, &height);

        description.size.width = width;
        description.size.height = height;
        description.mode = resolve_window_mode(window, description.mode);

        if (const char* title = SDL_GetWindowTitle(window); title)
        {
            description.title = title;
        }

        return description;
    }

    static bool apply_window_mode(SDL_Window* window, WindowMode mode)
    {
        bool success = true;
        const bool is_minimized =
            (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) == SDL_WINDOW_MINIMIZED;
        if (mode != WindowMode::Minimized && is_minimized)
        {
            success = SDL_RestoreWindow(window);
        }

        switch (mode)
        {
            case WindowMode::Windowed:
            {
                const bool fullscreen_cleared = SDL_SetWindowFullscreen(window, false);
                const bool bordered = SDL_SetWindowBordered(window, true);
                return success && fullscreen_cleared && bordered;
            }
            case WindowMode::Borderless:
            {
                const bool fullscreen_cleared = SDL_SetWindowFullscreen(window, false);
                const bool borderless = SDL_SetWindowBordered(window, false);
                return success && fullscreen_cleared && borderless;
            }
            case WindowMode::Fullscreen:
            {
                const bool fullscreen = SDL_SetWindowFullscreen(window, true);
                return success && fullscreen;
            }
            case WindowMode::Minimized:
                return SDL_MinimizeWindow(window);
        }

        return false;
    }

    static bool apply_window_description(SDL_Window* window, const WindowDescription& description)
    {
        bool succeeded = true;

        if (description.size.width > 0 && description.size.height > 0)
        {
            succeeded = SDL_SetWindowSize(window, description.size.width, description.size.height)
                        && succeeded;
        }

        succeeded = SDL_SetWindowTitle(window, description.title.c_str()) && succeeded;
        succeeded = apply_window_mode(window, description.mode) && succeeded;

        if (succeeded)
        {
            if (description.mode != WindowMode::Minimized)
            {
                SDL_ShowWindow(window);
            }
        }

        return succeeded;
    }

    // --------------------------------
    // SdlWindowingPlugin implementation
    // --------------------------------

    void SdlWindowingPlugin::on_attach(Application&)
    {
        if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
        {
            TBX_TRACE_ERROR("Failed to initialize SDL video subsystem. See SDL logs for details.");
            return;
        }
    }

    void SdlWindowingPlugin::on_detach()
    {
        for (auto& record : _windows)
        {
            if (record.sdl_window)
            {
                SDL_DestroyWindow(record.sdl_window);
            }
        }
        _windows.clear();
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }

    void SdlWindowingPlugin::on_update(const DeltaTime&)
    {
        SDL_Event event;
        if (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EventType::SDL_EVENT_WINDOW_CLOSE_REQUESTED)
            {
                auto* window = SDL_GetWindowFromID(event.window.windowID);
                SdlWindowRecord* record = find_record(window);
                if (record && record->tbx_window)
                {
                    record->tbx_window->close();
                }
            }
            else if (event.type == SDL_EventType::SDL_EVENT_QUIT)
            {
                for (auto& record : _windows)
                {
                    if (record.sdl_window)
                    {
                        SDL_DestroyWindow(record.sdl_window);
                    }
                }
                _windows.clear();
            }
        }
    }

    void SdlWindowingPlugin::on_message(Message& msg)
    {
        if (handle_message<CreateWindowRequest>(
                msg,
                [this](CreateWindowRequest& request)
                {
                    handle_create_window(request);
                }))
        {
            return;
        }

        if (handle_message<QueryWindowDescriptionRequest>(
                msg,
                [this](QueryWindowDescriptionRequest& request)
                {
                    handle_query_description(request);
                }))
        {
            return;
        }

        if (handle_message<ApplyWindowDescriptionRequest>(
                msg,
                [this](ApplyWindowDescriptionRequest& request)
                {
                    handle_apply_description(request);
                }))
        {
            return;
        }

        handle_message<WindowClosedEvent>(
            msg,
            [this](WindowClosedEvent& event)
            {
                handle_window_closed(event);
            });
    }

    void SdlWindowingPlugin::handle_create_window(CreateWindowRequest& request)
    {
        if (!request.window)
        {
            set_failure(request, "Create missing window reference.");
            request.state = MessageState::Handled;
            return;
        }

        if ((SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) != SDL_INIT_VIDEO)
        {
            set_failure(request, "SDL video subsystem is not initialized.");
            request.state = MessageState::Handled;
            return;
        }

        const WindowDescription& requestedDesc = request.description;
        if (requestedDesc.size.width <= 0 || requestedDesc.size.height <= 0)
        {
            set_failure(request, "Window dimensions must be positive.");
            request.state = MessageState::Handled;
            return;
        }

        Uint32 flags = SDL_WINDOW_RESIZABLE;
        if (requestedDesc.mode == WindowMode::Borderless)
        {
            flags |= SDL_WINDOW_BORDERLESS;
        }
        else if (requestedDesc.mode == WindowMode::Fullscreen)
        {
            flags |= SDL_WINDOW_FULLSCREEN;
        }

        SDL_Window* native = SDL_CreateWindow(
            requestedDesc.title.c_str(),
            requestedDesc.size.width,
            requestedDesc.size.height,
            flags);
        if (!native)
        {
            TBX_TRACE_ERROR("Failed to create SDL window. See SDL logs for details.");
            set_failure(request, "Failed to create SDL window.");
            request.state = MessageState::Handled;
            return;
        }

        if (!apply_window_description(native, requestedDesc))
        {
            TBX_TRACE_WARNING(
                "SDL window created but initial description could not be fully applied. See SDL "
                "logs for details.");
        }

        _windows.emplace_back();
        _windows.back().sdl_window = native;
        _windows.back().tbx_window = request.window;

        request.payload = native;
        request.state = MessageState::Handled;
    }

    void SdlWindowingPlugin::handle_query_description(QueryWindowDescriptionRequest& request) const
    {
        if (!request.window)
        {
            set_failure(request, "Query missing window reference.");
            request.state = MessageState::Handled;
            return;
        }

        const SdlWindowRecord* record = find_record(request.window);
        if (!record)
        {
            set_failure(request, "Window is not managed by SDL windowing.");
            request.state = MessageState::Handled;
            return;
        }

        const WindowDescription description = read_window_description(record->sdl_window);

        request.payload = description;
        request.state = MessageState::Handled;
    }

    void SdlWindowingPlugin::handle_apply_description(ApplyWindowDescriptionRequest& request) const
    {
        if (!request.window)
        {
            set_failure(request, "Apply missing window reference.");
            request.state = MessageState::Handled;
            return;
        }

        const SdlWindowRecord* record = find_record(request.window);
        if (!record)
        {
            set_failure(request, "Window is not managed by SDL windowing.");
            request.state = MessageState::Handled;
            return;
        }
        const WindowMode previous_mode = request.window->get_description().mode;
        if (!apply_window_description(record->sdl_window, request.description))
        {
            set_failure(request, "Failed to apply SDL window description.");
            request.state = MessageState::Handled;
            return;
        }

        const WindowDescription refreshed = read_window_description(record->sdl_window);
        request.payload = refreshed;
        if (request.window && previous_mode != refreshed.mode)
        {
            auto mode_changed = WindowModeChangedEvent(previous_mode, refreshed.mode);
            send_message(mode_changed);
        }

        request.state = MessageState::Handled;
    }

    void SdlWindowingPlugin::handle_window_closed(WindowClosedEvent& event)
    {
        if (!event.window)
        {
            return;
        }

        auto it = std::ranges::find_if(
            _windows,
            [&event](const SdlWindowRecord& record)
            {
                return record.tbx_window == event.window;
            });
        if (it == _windows.end())
        {
            return;
        }

        if (it->sdl_window)
        {
            SDL_DestroyWindow(it->sdl_window);
        }

        _windows.erase(it);
        event.state = MessageState::Handled;
    }

    void SdlWindowingPlugin::set_failure(Message& message, std::string_view reason)
    {
        message.result.flag_failure(reason.empty() ? std::string() : std::string(reason));
        message.state = MessageState::Failed;
    }

    SdlWindowRecord* SdlWindowingPlugin::find_record(const SDL_Window* window)
    {
        auto it = std::ranges::find_if(
            _windows,
            [&window](const SdlWindowRecord& record)
            {
                return record.sdl_window == window;
            });
        if (it == _windows.end())
        {
            return nullptr;
        }

        return &(*it);
    }

    SdlWindowRecord* SdlWindowingPlugin::find_record(const Window* window)
    {
        auto it = std::ranges::find_if(
            _windows,
            [&window](const SdlWindowRecord& record)
            {
                return record.tbx_window == window;
            });
        if (it == _windows.end())
        {
            return nullptr;
        }

        return &(*it);
    }

    const SdlWindowRecord* SdlWindowingPlugin::find_record(const SDL_Window* window) const
    {
        auto it = std::ranges::find_if(
            _windows,
            [&window](const SdlWindowRecord& record)
            {
                return record.sdl_window == window;
            });
        if (it == _windows.end())
        {
            return nullptr;
        }

        return &(*it);
    }

    const SdlWindowRecord* SdlWindowingPlugin::find_record(const Window* window) const
    {
        auto it = std::ranges::find_if(
            _windows,
            [&window](const SdlWindowRecord& record)
            {
                return record.tbx_window == window;
            });
        if (it == _windows.end())
        {
            return nullptr;
        }

        return &(*it);
    }
}
