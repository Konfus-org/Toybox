#pragma once
#include "tbx/memory/smart_pointers.h"
#include "tbx/messages/commands/window_commands.h"
#include "tbx/os/window.h"
#include "tbx/plugin_api/plugin.h"
#include "tbx/state/result.h"
#include <string_view>
#include <vector>

struct SDL_Window;

namespace tbx::plugins::sdlwindowing
{
    class SdlWindowingPlugin final : public Plugin
    {
       public:
        void on_attach(const ApplicationContext& context) override;
        void on_detach() override;
        void on_update(const DeltaTime& dt) override;
        void on_message(const Message& msg) override;

       private:
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

        void handle_create_window(CreateWindowCommand& command);
        void handle_query_description(QueryWindowDescriptionCommand& command);
        void handle_apply_description(ApplyWindowDescriptionCommand& command);
        static void set_failure(Result* result, std::string_view reason);
        SdlWindowRecord* find_record(const Window& window) const;

        std::vector<Scope<SdlWindowRecord>> _windows;
    };
}
