#pragma once
#include "tbx/plugin_api/plugin.h"

namespace ecs_example
{
    using namespace tbx;
    class ExampleRuntimePlugin final : public Plugin
    {
      public:
        void on_attach(IPluginHost& host) override;
        void on_update(const DeltaTime& dt) override;

      private:
        float _elapsed_seconds = 0.0F;
    };
}
