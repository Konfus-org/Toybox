#include "tbx/logging/log_macros.h"
#include "tbx/memory/casting.h"
#include "tbx/memory/smart_pointers.h"
#include "tbx/messages/commands/window_commands.h"
#include "tbx/plugin_api/plugin.h"
#include "tbx/windowing/window.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tbx::plugins::sdlwindowing
{
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

    static WindowDescription read_window_description(SDL_Window* window, const WindowDescription& fallback)
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
            succeeded = SDL_SetWindowSize(window, description.size.width, description.size.height) && succeeded;
        }

        succeeded = SDL_SetWindowTitle(window, description.title.c_str()) && succeeded;
        succeeded = apply_window_mode(window, description.mode) && succeeded;

        if (succeeded)
        {
            SDL_ShowWindow(window);
        }

        return succeeded;
    }

    struct SdlWindowRecord
    {
        SdlWindowRecord(IMessageDispatcher& dispatcher, SDL_Window* native_window, const WindowDescription& description)
            : window(dispatcher, static_cast<void*>(native_window), description)
            , description(description)
            , native(native_window)
        {
        }

        Window window;
        WindowDescription description = {};
        SDL_Window* native = nullptr;
    };

    class SdlWindowingPlugin final : public Plugin
    {
    public:
        void on_attach(const ApplicationContext&, IMessageDispatcher& dispatcher) override
        {
            _dispatcher = &dispatcher;

            if ((SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) != SDL_INIT_VIDEO)
            {
                if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
                {
                    TBX_TRACE_ERROR("Failed to initialize SDL video subsystem. See SDL logs for details.");
                    return;
                }

                _owns_video = true;
                TBX_TRACE_INFO("SDL windowing initialized the video subsystem.");
            }
        }

        void on_detach() override
        {
            for (auto& record : _windows)
            {
                if (record->native)
                {
                    SDL_DestroyWindow(record->native);
                    record->native = nullptr;
                }
            }
            _windows.clear();

            if (_owns_video)
            {
                SDL_QuitSubSystem(SDL_INIT_VIDEO);
                _owns_video = false;
            }

            _dispatcher = nullptr;
        }

        void on_update(const DeltaTime&) override
        {
            if (SDL_WasInit(SDL_INIT_VIDEO))
            {
                SDL_PumpEvents();
            }
        }

        void on_message(const Message& msg) override
        {
            CreateWindowCommand* create = nullptr;
            if (try_as(msg, &create))
            {
                handle_create_window(*create);
                return;
            }

            QueryWindowDescriptionCommand* query = nullptr;
            if (try_as(msg, &query))
            {
                handle_query_description(*query);
                return;
            }

            ApplyWindowDescriptionCommand* apply = nullptr;
            if (try_as(msg, &apply))
            {
                handle_apply_description(*apply);
            }
        }

    private:
        void handle_create_window(CreateWindowCommand& command)
        {
            Result* result = command.get_result();

            if (!_dispatcher)
            {
                set_failure(result, "Window dispatcher is not available.");
                command.is_handled = true;
                return;
            }

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

            SDL_Window* native = SDL_CreateWindow(requested.title.c_str(), requested.size.width, requested.size.height, flags);
            if (!native)
            {
                TBX_TRACE_ERROR("Failed to create SDL window. See SDL logs for details.");
                set_failure(result, "Failed to create SDL window.");
                command.is_handled = true;
                return;
            }

            if (!apply_window_description(native, requested))
            {
                TBX_TRACE_WARNING("SDL window created but initial description could not be fully applied. See SDL logs for details.");
            }

            const WindowDescription description = read_window_description(native, requested);

            auto record = tbx::make_scope<SdlWindowRecord>(*_dispatcher, native, description);

            if (result)
            {
                result->set_payload<Window*>(&record->window);
                result->set_status(ResultStatus::Handled);
            }

            _windows.push_back(std::move(record));
            command.is_handled = true;
        }

        static void set_failure(Result* result, std::string_view reason)
        {
            if (!result)
            {
                return;
            }
            result->reset_payload();
            result->set_status(ResultStatus::Failed, std::string(reason));
        }

        void handle_query_description(QueryWindowDescriptionCommand& command)
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

            const WindowDescription description = read_window_description(record->native, record->description);
            record->description = description;
            if (result)
            {
                result->set_payload(description);
                result->set_status(ResultStatus::Handled);
            }

            command.is_handled = true;
        }

        void handle_apply_description(ApplyWindowDescriptionCommand& command)
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

            const WindowDescription refreshed = read_window_description(record->native, command.description);
            record->description = refreshed;
            if (result)
            {
                result->set_payload(refreshed);
                result->set_status(ResultStatus::Handled);
            }

            command.is_handled = true;
        }

        SdlWindowRecord* find_record(const Window& window) const
        {
            auto it = std::find_if(_windows.begin(), _windows.end(), [&window](const Scope<SdlWindowRecord>& record) {
                return &record->window == &window;
            });
            if (it == _windows.end())
            {
                return nullptr;
            }
            return it->get();
        }

        std::vector<Scope<SdlWindowRecord>> _windows = {};
        IMessageDispatcher* _dispatcher = nullptr;
        bool _owns_video = false;
    };

    TBX_REGISTER_PLUGIN(CreateSdlWindowingPlugin, SdlWindowingPlugin);
}

