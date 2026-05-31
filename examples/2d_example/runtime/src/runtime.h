#pragma once
#include "tbx/interfaces/plugin.h"
#include "tbx/types/assets/world.h"

namespace two_d_example
{
    /// @brief
    /// Purpose: Drives the 2D example scene setup and per-frame toy animation.
    /// @details
    /// Ownership: Owned by the plugin host; stores no owning references to host-managed systems.
    /// Thread Safety: Not thread-safe; the host invokes lifecycle callbacks on the main thread.
    [[tbx::plugin]];
    [[tbx::name("TwoDExampleRuntime")]];
    [[tbx::version("1.0.0")]];
    [[tbx::category("gameplay")]];
    class TwoDExampleRuntimePlugin final : public tbx::Plugin
    {
      public:
        void on_attach(tbx::ServiceProvider& service_provider) override;
        void on_detach(tbx::ServiceProvider& service_provider) override;
        void on_update(const tbx::DeltaTime& dt) override;

      private:
        float _elapsed_seconds = 0.0f;
        std::unique_ptr<tbx::World> _world = {};
    };
}
