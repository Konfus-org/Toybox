#pragma once
#include "tbx/app/window.h"
#include "tbx/app/window_requests.h"
#include "tbx/common/smart_pointers.h"
#include "tbx/plugin_api/plugin.h"
#include <SDL3/SDL.h>
#include <string_view>
#include <vector>

namespace tbx::plugins::sdlwindowing
{
    struct SdlWindowRecord
    {
        SDL_Window* window = nullptr;
        uuid id = {};
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
        void handle_open_window(OpenWindowRequest& request) const;
        void handle_query_description(QueryWindowDescriptionRequest& request) const;
        void handle_apply_description(ApplyWindowDescriptionRequest& request) const;
        void handle_close_window(CloseWindowRequest& request);
        static void set_failure(Message& message, std::string_view reason);
        SdlWindowRecord find_record(const SDL_Window* window) const;
        SdlWindowRecord find_record(const uuid& id) const;

        std::vector<SdlWindowRecord> _windows;
    };
}
