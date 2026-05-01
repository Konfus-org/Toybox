#pragma once
#include "tbx/interfaces/plugin.h"
#include "tbx/systems/plugin_api/plugin_export.h"
#include <functional>
#include <optional>

namespace jolt_physics
{
    class TBX_PLUGIN_API JoltPhysicsPlugin final : public tbx::Plugin
    {
      public:
        void on_attach(tbx::ServiceProvider& service_provider) override;
        void on_detach() override;

      private:
        std::optional<std::reference_wrapper<tbx::ServiceProvider>> _service_provider =
            std::nullopt;
    };
}
