#pragma once
#include "tbx/plugin_api/plugin.h"

namespace tbx
{
    class ECS;
}

namespace tbx::examples
{
    class ExampleRuntimePlugin final : public Plugin
    {
      public:
        void on_attach(Application& host) override;
        void on_detach() override;
        void on_update(const DeltaTime& dt) override;
        void on_recieve_message(Message& msg) override;

      private:
        ECS* _ecs;
    };
}
