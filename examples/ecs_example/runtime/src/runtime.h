#pragma once
#include "tbx/ecs/entity.h"
#include "tbx/examples/camera_controller.h"
#include "tbx/plugin_api/plugin.h"
#include <string>

namespace tbx::examples
{
    class ExampleRuntimePlugin final : public Plugin
    {
      public:
        void on_attach(IPluginHost& host) override;
        void on_update(const DeltaTime& dt) override;

      private:
        float _elapsed_seconds = 0.0F;
    };
}
