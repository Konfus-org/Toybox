#pragma once
#include "tbx/plugin_api/plugin.h"
#include <SDL3/SDL_log.h>

namespace tbx::plugins::sdladapter
{
    class SdlGlContextPlugin final : public Plugin
    {
      public:
        void on_attach(Application& host) override;
        void on_detach() override;
        void on_update(const DeltaTime& dt) override;
    };
}
