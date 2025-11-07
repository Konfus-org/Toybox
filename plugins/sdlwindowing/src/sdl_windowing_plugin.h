#pragma once
#include "tbx/memory/smart_pointers.h"
#include "tbx/messages/commands/window_commands.h"
#include "tbx/os/window.h"
#include "tbx/plugin_api/plugin.h"
#include "tbx/state/result.h"
#include <SDL3/SDL.h>
#include <string_view>
#include <vector>

namespace tbx::plugins::sdlwindowing
{
    struct SdlWindowRecord
    {
        SdlWindowRecord(
            IMessageDispatcher& dispatcher,
            SDL_Window* native_window,
            const WindowDescription& description);

        Window window;
        WindowDescription description = {};
        SDL_Window* native = nullptr;
    };

    class SdlWindowingPlugin final : public Plugin
    {
      public:
        void on_attach(Application& host) override;
        void on_detach() override;
        void on_update(const DeltaTime& dt) override;
        void on_message(Message& msg) override;

      private:
        void handle_create_window(CreateWindowCommand& command);
        void handle_open_window(OpenWindowCommand& command) const;
        void handle_query_description(QueryWindowDescriptionCommand& command) const;
        void handle_apply_description(ApplyWindowDescriptionCommand& command) const;
        void handle_close_window(CloseWindowCommand& command);
        static void set_failure(Result* result, std::string_view reason);
        SdlWindowRecord* find_record(const Window& window) const;

        std::vector<Scope<SdlWindowRecord>> _windows;
    };
}
