#include "sdl_windowing_plugin.h"
#include "tbx/app/application.h"
#include "tbx/debug/macros.h"
#include "tbx/messages/events/window_events.h"
#include "tbx/tsl/casting.h"
#include "tbx/tsl/smart_pointers.h"
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

    void SdlWindowingPlugin::on_attach(Application&)
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
        if (auto* create = tbx::as<CreateWindowCommand>(&msg))
        {
            handle_create_window(*create);
            return;
        }

        if (auto* open = tbx::as<OpenWindowCommand>(&msg))
        {
            handle_open_window(*open);
            return;
        }

        if (auto* query = tbx::as<QueryWindowDescriptionCommand>(&msg))
        {
            handle_query_description(*query);
            return;
        }

        if (auto* apply = tbx::as<ApplyWindowDescriptionCommand>(&msg))
        {
            handle_apply_description(*apply);
            return;
        }

        if (auto* close = tbx::as<CloseWindowCommand>(&msg))
        {
            handle_close_window(*close);
        }
    }

    void SdlWindowingPlugin::handle_create_window(CreateWindowCommand& command)
    {

        if ((SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) != SDL_INIT_VIDEO)
        {
            set_failure(command, "SDL video subsystem is not initialized.");
            command.state = MessageState::Handled;
            return;
        }

        const WindowDescription& requested = command.description;
        if (requested.size.width <= 0 || requested.size.height <= 0)
        {
            set_failure(command, "Window dimensions must be positive.");
            command.state = MessageState::Handled;
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
            set_failure(command, "Failed to create SDL window.");
            command.state = MessageState::Handled;
            return;
        }

        if (!apply_window_description(native, requested))
        {
            TBX_TRACE_WARNING(
                "SDL window created but initial description could not be fully applied. See SDL "
                "logs for details.");
        }

        const WindowDescription description = read_window_description(native, requested);

        tbx::Scope<SdlWindowRecord> record(get_dispatcher(), native, description);

        command.payload = &record->window;

        _windows.push_back(std::move(record));
        command.state = MessageState::Handled;
    }

    void SdlWindowingPlugin::set_failure(Message& message, std::string_view reason)
    {
        message.payload.reset();
        message.result.flag_failure(reason.empty() ? std::string() : std::string(reason));
        message.state = MessageState::Failed;
    }

    void SdlWindowingPlugin::handle_query_description(QueryWindowDescriptionCommand& command) const
    {
        if (!command.window)
        {
            set_failure(command, "Query missing window reference.");
            command.state = MessageState::Handled;
            return;
        }

        SdlWindowRecord* record = find_record(*command.window);
        if (!record)
        {
            set_failure(command, "Window is not managed by SDL windowing.");
            command.state = MessageState::Handled;
            return;
        }

        const WindowDescription description =
            read_window_description(record->native, record->description);
        record->description = description;
        command.payload = description;

        command.state = MessageState::Handled;
    }

    void SdlWindowingPlugin::handle_apply_description(ApplyWindowDescriptionCommand& command) const
    {
        if (!command.window)
        {
            set_failure(command, "Apply missing window reference.");
            command.state = MessageState::Handled;
            return;
        }

        SdlWindowRecord* record = find_record(*command.window);
        if (!record)
        {
            set_failure(command, "Window is not managed by SDL windowing.");
            command.state = MessageState::Handled;
            return;
        }

        const WindowMode previous_mode = record->description.mode;
        if (!apply_window_description(record->native, command.description))
        {
            set_failure(command, "Failed to apply SDL window description.");
            command.state = MessageState::Handled;
            return;
        }

        const WindowDescription refreshed =
            read_window_description(record->native, command.description);
        record->description = refreshed;
        command.payload = refreshed;

        if (command.window && previous_mode != refreshed.mode)
        {
            WindowModeChangedEvent mode_changed(command.window, previous_mode, refreshed.mode);
            send_message(mode_changed);
        }

        command.state = MessageState::Handled;
    }

    void SdlWindowingPlugin::handle_open_window(OpenWindowCommand& command) const
    {
        if (!command.window)
        {
            set_failure(command, "Open missing window reference.");
            command.state = MessageState::Handled;
            return;
        }

        WindowOpenedEvent event(command.window, command.description);
        send_message(event);

        command.payload.reset();

        command.state = MessageState::Handled;
    }

    void SdlWindowingPlugin::handle_close_window(CloseWindowCommand& command)
    {
        if (!command.window)
        {
            set_failure(command, "Close missing window reference.");
            command.state = MessageState::Handled;
            return;
        }

        auto it = std::ranges::find_if(
            _windows,
            [&command](const tbx::Scope<SdlWindowRecord>& record)
            {
                return record && &record->window == command.window;
            });
        if (it == _windows.end())
        {
            set_failure(command, "Window is not managed by SDL windowing.");
            command.state = MessageState::Handled;
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

        command.payload.reset();

        command.state = MessageState::Handled;
    }

    SdlWindowRecord* SdlWindowingPlugin::find_record(const Window& window) const
    {
        auto it = std::ranges::find_if(
            _windows,
            [&window](const tbx::Scope<SdlWindowRecord>& record)
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
