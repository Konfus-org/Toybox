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
    [[tbx::plugin(
        name = "TwoDExampleRuntime",
        version = "1.0.0",
        category = tbx::PluginCategory::GAMEPLAY)]];
    class TwoDExampleRuntimePlugin final : public tbx::Plugin
    {
      public:
        void on_attach() override;
        void on_detach() override;
        void on_update(const tbx::DeltaTime& dt) override;

      private:
        float _elapsed_seconds = 0.0f;
        std::unique_ptr<tbx::World> _world = {};
    };
}
