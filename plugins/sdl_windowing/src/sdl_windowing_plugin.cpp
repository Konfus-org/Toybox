#include "sdl_windowing_plugin.h"
#include "tbx/app/application.h"
#include "tbx/app/window_events.h"
#include "tbx/common/casting.h"
#include "tbx/common/smart_pointers.h"
#include "tbx/debugging/macros.h"
#include <algorithm>
#include <memory>
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
                /* find_record(window);
                 auto closed_event = WindowClosedEvent(it->get()->window);*/
            }
            else if (event.type == SDL_EventType::SDL_EVENT_QUIT)
            {
                _windows.clear();
            }
        }
    }

    void SdlWindowingPlugin::on_message(Message& msg)
    {
        if (auto* create = as<CreateWindowRequest>(&msg))
        {
            handle_create_window(*create);
            return;
        }

        if (auto* open = as<OpenWindowRequest>(&msg))
        {
            handle_open_window(*open);
            return;
        }

        if (auto* query = as<QueryWindowDescriptionRequest>(&msg))
        {
            handle_query_description(*query);
            return;
        }

        if (auto* apply = as<ApplyWindowDescriptionRequest>(&msg))
        {
            handle_apply_description(*apply);
            return;
        }

        if (auto* close = as<CloseWindowRequest>(&msg))
        {
            handle_close_window(*close);
        }
    }

    void SdlWindowingPlugin::handle_create_window(CreateWindowRequest& request)
    {
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

        _windows.emplace_back(native, requestedDesc.id);
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

        SdlWindowRecord record = find_record(request.window->get_description().id);
        const WindowDescription description = read_window_description(record.window);

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

        SdlWindowRecord record = find_record(request.window->get_description().id);
        const WindowMode previous_mode = request.window->get_description().mode;
        if (!apply_window_description(record.window, request.description))
        {
            set_failure(request, "Failed to apply SDL window description.");
            request.state = MessageState::Handled;
            return;
        }

        const WindowDescription refreshed = read_window_description(record.window);
        request.payload = refreshed;
        if (request.window && previous_mode != refreshed.mode)
        {
            auto mode_changed = WindowModeChangedEvent(previous_mode, refreshed.mode);
            send_message(mode_changed);
        }

        request.state = MessageState::Handled;
    }

    void SdlWindowingPlugin::handle_open_window(OpenWindowRequest& request) const
    {
        if (!request.window)
        {
            set_failure(request, "Open missing window reference.");
            request.state = MessageState::Handled;
            return;
        }

        request.state = MessageState::Handled;
    }

    void SdlWindowingPlugin::handle_close_window(CloseWindowRequest& request)
    {
        if (!request.window)
        {
            set_failure(request, "Close missing window reference.");
            return;
        }

        auto it = std::ranges::find_if(
            _windows,
            [&request](const Scope<SdlWindowRecord>& record)
            {
                return record->id == request.window->get_description().id;
            });
        if (it == _windows.end())
        {
            set_failure(request, "Window is not managed by SDL windowing.");
            return;
        }

        const SdlWindowRecord& record = *it;
        if (record.window)
        {
            SDL_DestroyWindow(record.window);
        }

        _windows.erase(it);
        request.payload.reset();
        request.state = MessageState::Handled;
    }

    void SdlWindowingPlugin::set_failure(Message& message, std::string_view reason)
    {
        message.result.flag_failure(reason.empty() ? std::string() : std::string(reason));
        message.state = MessageState::Failed;
    }

    SdlWindowRecord SdlWindowingPlugin::find_record(const SDL_Window* window) const
    {
        auto it = std::ranges::find_if(
            _windows,
            [&window](const Scope<SdlWindowRecord>& record)
            {
                return record->window == window;
            });
        if (it == _windows.end())
        {
            TBX_ASSERT(false, "No SDL window record found for the given window.");
            return {};
        }
        return *it;
    }

    SdlWindowRecord SdlWindowingPlugin::find_record(const uuid& id) const
    {
        auto it = std::ranges::find_if(
            _windows,
            [&id](const Scope<SdlWindowRecord>& record)
            {
                return record->id == id;
            });
        if (it == _windows.end())
        {
            TBX_ASSERT(false, "No SDL window record found for the given ID.");
            return {};
        }

        return *it;
    }
}
