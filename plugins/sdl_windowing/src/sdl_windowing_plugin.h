#pragma once
#include "tbx/app/window.h"
#include "tbx/app/window_requests.h"
#include "tbx/app/window_events.h"
#include "tbx/plugin_api/plugin.h"
#include <SDL3/SDL.h>
#include <string_view>
#include <vector>

namespace tbx::plugins::sdlwindowing
{
    struct SdlWindowRecord
    {
        SDL_Window* sdl_window = nullptr;
        Window* tbx_window = nullptr;
    };

    class SdlWindowingPlugin final : public Plugin
    {
      public:
        void on_attach(Application& host) override;
        void on_detach() override;
        void on_update(const DeltaTime& dt) override;
        void on_message(Message& msg) override;

      private:
        void handle_create_window(CreateWindowRequest& request);
        void handle_query_description(QueryWindowDescriptionRequest& request) const;
        void handle_apply_description(ApplyWindowDescriptionRequest& request) const;
        void handle_window_closed(WindowClosedEvent& event);
        static void set_failure(Message& message, std::string_view reason);
        SdlWindowRecord* find_record(const SDL_Window* window);
        SdlWindowRecord* find_record(const Window* window);
        const SdlWindowRecord* find_record(const SDL_Window* window) const;
        const SdlWindowRecord* find_record(const Window* window) const;

        std::vector<SdlWindowRecord> _windows;
    };
}
