#include "sdl_windowing_plugin.h"
#include "tbx/application.h"
#include "tbx/debug/macros.h"
#include "tbx/memory/casting.h"
#include "tbx/messages/events/window_events.h"
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

    static WindowDescription read_window_description(
        SDL_Window* window,
        const WindowDescription& fallback)
    {
        WindowDescription description = fallback;

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
    // SdlWindowRecord implementation
    // --------------------------------

    SdlWindowRecord::SdlWindowRecord(
        IMessageDispatcher& dispatcher,
        SDL_Window* native_window,
        const WindowDescription& description)
        : window(dispatcher, description)
        , description(description)
        , native(native_window)
    {
    }

    // --------------------------------
    // SdlWindowingPlugin implementation
    // --------------------------------

    void SdlWindowingPlugin::on_attach(const ApplicationContext&)
    {
        if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
        {
            TBX_TRACE_ERROR("Failed to initialize SDL video subsystem. See SDL logs for details.");
            return;
        }

        // TESTING CODE:
        auto window_command = CreateWindowCommand(WindowDescription());
        handle_create_window(window_command);
    }

    void SdlWindowingPlugin::on_detach()
    {
        for (const auto& record : _windows)
        {
            if (record && record->native)
            {
                SDL_DestroyWindow(record->native);
                record->native = nullptr;
            }

            if (record)
            {
                if (record->window.is_open())
                {
                    WindowClosedEvent closed(&record->window);
                    get_dispatcher().send(closed);
                }
            }
        }
        _windows.clear();

        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }

    void SdlWindowingPlugin::on_update(const DeltaTime&) {}

    void SdlWindowingPlugin::on_message(Message& msg)
    {
        if (auto* create = as<CreateWindowCommand>(&msg))
        {
            handle_create_window(*create);
            return;
        }

        if (auto* open = as<OpenWindowCommand>(&msg))
        {
            handle_open_window(*open);
            return;
        }

        if (auto* query = as<QueryWindowDescriptionCommand>(&msg))
        {
            handle_query_description(*query);
            return;
        }

        if (auto* apply = as<ApplyWindowDescriptionCommand>(&msg))
        {
            handle_apply_description(*apply);
            return;
        }

        if (auto* close = as<CloseWindowCommand>(&msg))
        {
            handle_close_window(*close);
        }
    }

    void SdlWindowingPlugin::handle_create_window(CreateWindowCommand& command)
    {
        Result* result = command.get_result();

        if ((SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) != SDL_INIT_VIDEO)
        {
            set_failure(result, "SDL video subsystem is not initialized.");
            command.is_handled = true;
            return;
        }

        const WindowDescription& requested = command.description;
        if (requested.size.width <= 0 || requested.size.height <= 0)
        {
            set_failure(result, "Window dimensions must be positive.");
            command.is_handled = true;
            return;
        }

        Uint32 flags = SDL_WINDOW_RESIZABLE;
        if (requested.mode == WindowMode::Borderless)
        {
            flags |= SDL_WINDOW_BORDERLESS;
        }
        else if (requested.mode == WindowMode::Fullscreen)
        {
            flags |= SDL_WINDOW_FULLSCREEN;
        }

        SDL_Window* native = SDL_CreateWindow(
            requested.title.c_str(),
            requested.size.width,
            requested.size.height,
            flags);
        if (!native)
        {
            TBX_TRACE_ERROR("Failed to create SDL window. See SDL logs for details.");
            set_failure(result, "Failed to create SDL window.");
            command.is_handled = true;
            return;
        }

        if (!apply_window_description(native, requested))
        {
            TBX_TRACE_WARNING(
                "SDL window created but initial description could not be fully applied. See SDL "
                "logs for details.");
        }

        const WindowDescription description = read_window_description(native, requested);

        auto record = make_scope<SdlWindowRecord>(get_dispatcher(), native, description);

        if (result)
        {
            result->set_payload<Window*>(&record->window);
            result->set_status(ResultStatus::Handled);
        }

        _windows.push_back(std::move(record));
        command.is_handled = true;
    }

    void SdlWindowingPlugin::set_failure(Result* result, std::string_view reason)
    {
        result->reset_payload();
        result->set_status(ResultStatus::Failed, std::string(reason));
    }

    void SdlWindowingPlugin::handle_query_description(QueryWindowDescriptionCommand& command) const
    {
        Result* result = command.get_result();

        if (!command.window)
        {
            set_failure(result, "Query missing window reference.");
            command.is_handled = true;
            return;
        }

        SdlWindowRecord* record = find_record(*command.window);
        if (!record)
        {
            set_failure(result, "Window is not managed by SDL windowing.");
            command.is_handled = true;
            return;
        }

        const WindowDescription description =
            read_window_description(record->native, record->description);
        record->description = description;
        if (result)
        {
            result->set_payload(description);
            result->set_status(ResultStatus::Handled);
        }

        command.is_handled = true;
    }

    void SdlWindowingPlugin::handle_apply_description(ApplyWindowDescriptionCommand& command) const
    {
        Result* result = command.get_result();

        if (!command.window)
        {
            set_failure(result, "Apply missing window reference.");
            command.is_handled = true;
            return;
        }

        SdlWindowRecord* record = find_record(*command.window);
        if (!record)
        {
            set_failure(result, "Window is not managed by SDL windowing.");
            command.is_handled = true;
            return;
        }

        const WindowMode previous_mode = record->description.mode;
        if (!apply_window_description(record->native, command.description))
        {
            set_failure(result, "Failed to apply SDL window description.");
            command.is_handled = true;
            return;
        }

        const WindowDescription refreshed =
            read_window_description(record->native, command.description);
        record->description = refreshed;
        if (result)
        {
            result->set_payload(refreshed);
            result->set_status(ResultStatus::Handled);
        }

        if (command.window && previous_mode != refreshed.mode)
        {
            WindowModeChangedEvent mode_changed(command.window, previous_mode, refreshed.mode);
            send_message(mode_changed);
        }

        command.is_handled = true;
    }

    void SdlWindowingPlugin::handle_open_window(OpenWindowCommand& command) const
    {
        Result* result = command.get_result();

        if (!command.window)
        {
            set_failure(result, "Open missing window reference.");
            command.is_handled = true;
            return;
        }

        WindowOpenedEvent event(command.window, command.description);
        send_message(event);

        if (result)
        {
            result->reset_payload();
            result->set_status(ResultStatus::Handled);
        }

        command.is_handled = true;
    }

    void SdlWindowingPlugin::handle_close_window(CloseWindowCommand& command)
    {
        Result* result = command.get_result();

        if (!command.window)
        {
            set_failure(result, "Close missing window reference.");
            command.is_handled = true;
            return;
        }

        auto it = std::ranges::find_if(
            _windows,
            [&command](const Scope<SdlWindowRecord>& record)
            {
                return record && &record->window == command.window;
            });
        if (it == _windows.end())
        {
            set_failure(result, "Window is not managed by SDL windowing.");
            command.is_handled = true;
            return;
        }

        SdlWindowRecord* record = it->get();
        if (record->native)
        {
            SDL_DestroyWindow(record->native);
            record->native = nullptr;
        }

        if (command.window)
        {
            WindowClosedEvent closed(command.window);
            send_message(closed);
        }

        _windows.erase(it);

        if (result)
        {
            result->reset_payload();
            result->set_status(ResultStatus::Handled);
        }

        command.is_handled = true;
    }

    SdlWindowRecord* SdlWindowingPlugin::find_record(const Window& window) const
    {
        auto it = std::ranges::find_if(
            _windows,
            [&window](const Scope<SdlWindowRecord>& record)
            {
                return &record->window == &window;
            });
        if (it == _windows.end())
        {
            return nullptr;
        }
        return it->get();
    }
}
