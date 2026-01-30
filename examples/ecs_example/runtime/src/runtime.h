#pragma once
#include "tbx/ecs/entities.h"
#include "tbx/plugin_api/plugin.h"

namespace tbx::examples
{
    class ExampleRuntimePlugin final : public Plugin
    {
      public:
        void on_attach(IPluginHost& host) override;
        void on_detach() override;
        void on_update(const DeltaTime& dt) override;
        void on_recieve_message(Message& msg) override;

      private:
        EntityManager* _entity_manager = nullptr;
    };
}
