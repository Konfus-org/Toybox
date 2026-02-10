#pragma once
#include "tbx/ecs/entities.h"
#include "tbx/plugin_api/plugin.h"

namespace tbx::examples
{
    class ExampleRuntimePlugin final : public Plugin
    {
      public:
        void on_attach(IPluginHost& host) override;
        void on_update(const DeltaTime& dt) override;

      private:
        EntityRegistry* _entity_manager = nullptr;
        float _elapsed_seconds = 0.0f;
    };
}
