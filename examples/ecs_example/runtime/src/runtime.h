#pragma once
#include "tbx/plugin_api/plugin.h"

namespace ecs_example
{
    class ExampleRuntimePlugin final : public tbx::Plugin
    {
      public:
        void on_attach(tbx::IPluginHost& host) override;
        void on_update(const tbx::DeltaTime& dt) override;

      private:
        float _elapsed_seconds = 0.0F;
    };
}
