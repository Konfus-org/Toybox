#include "sdl_windowing_plugin.h"
#include "tbx/application.h"
#include "tbx/debug/log_macros.h"
#include "tbx/memory/casting.h"
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
        const Uint32 flags = SDL_GetWindowFlags(window);
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
        switch (mode)
        {
            case WindowMode::Windowed:
            {
                const bool fullscreen_cleared = SDL_SetWindowFullscreen(window, false);
                const bool bordered = SDL_SetWindowBordered(window, true);
                return fullscreen_cleared && bordered;
            }
            case WindowMode::Borderless:
            {
                const bool fullscreen_cleared = SDL_SetWindowFullscreen(window, false);
                const bool borderless = SDL_SetWindowBordered(window, false);
                return fullscreen_cleared && borderless;
            }
            case WindowMode::Fullscreen:
                return SDL_SetWindowFullscreen(window, true);
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
            SDL_ShowWindow(window);
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
        : window(dispatcher, static_cast<void*>(native_window), description)
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

        auto window_command = CreateWindowCommand(WindowDescription());
        handle_create_window(window_command);
    }

    void SdlWindowingPlugin::on_detach()
    {
        for (const auto& record : _windows)
        {
            if (record->native)
            {
                SDL_DestroyWindow(record->native);
                record->native = nullptr;
            }
        }
        _windows.clear();

        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }

    void SdlWindowingPlugin::on_update(const DeltaTime&) {}

    void SdlWindowingPlugin::on_message(const Message& msg)
    {
        if (auto* create = as<CreateWindowCommand>(const_cast<Message*>(&msg)))
        {
            handle_create_window(*create);
            return;
        }

        if (auto* query = as<QueryWindowDescriptionCommand>(const_cast<Message*>(&msg)))
        {
            handle_query_description(*query);
            return;
        }

        if (auto* apply = as<ApplyWindowDescriptionCommand>(const_cast<Message*>(&msg)))
        {
            handle_apply_description(*apply);
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
        if (!result)
        {
            return;
        }

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

        command.is_handled = true;
    }

    SdlWindowRecord* SdlWindowingPlugin::find_record(const Window& window) const
    {
        auto it = std::find_if(
            _windows.begin(),
            _windows.end(),
            [&window](const Scope<SdlWindowRecord>& record) { return &record->window == &window; });
        if (it == _windows.end())
        {
            return nullptr;
        }
        return it->get();
    }
}
