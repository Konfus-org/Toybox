#pragma once
#include "demo_scene.h"
#include "tbx/core/interfaces/plugin.h"
#include <memory>

namespace three_d_example
{
    class ThreeDExampleRuntimePlugin final : public tbx::Plugin
    {
      public:
        void on_attach(tbx::ServiceProvider& service_provider) override;
        void on_detach() override;
        void on_update(const tbx::DeltaTime& dt) override;

      private:
        std::unique_ptr<DemoScene> _scene = {};
    };
}
