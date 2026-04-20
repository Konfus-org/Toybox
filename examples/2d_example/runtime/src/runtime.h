#pragma once
#include "tbx/ecs/entity_registry.h"
#include "tbx/plugin_api/plugin.h"

namespace two_d_example
{
    /// @brief
    /// Purpose: Drives the 2D example scene setup and per-frame toy animation.
    /// @details
    /// Ownership: Owned by the plugin host; stores no owning references to host-managed systems.
    /// Thread Safety: Not thread-safe; the host invokes lifecycle callbacks on the main thread.
    class TwoDExampleRuntimePlugin final : public tbx::Plugin
    {
      public:
        void on_attach(tbx::ServiceProvider& service_provider) override;
        void on_detach() override;
        void on_update(const tbx::DeltaTime& dt) override;

      private:
        float _elapsed_seconds = 0.0f;
        tbx::EntityRegistry* _entity_registry = nullptr;
    };
}
