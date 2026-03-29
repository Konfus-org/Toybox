#pragma once
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
        /// @brief
        /// Purpose: Creates the example camera and renderable entities when the plugin attaches.
        /// @details
        /// Ownership: Does not retain ownership of the provided host; only uses it during the call
        /// and through the base host reference.
        /// Thread Safety: Not thread-safe; call from the main thread during plugin attachment.

        void on_attach(tbx::IPluginHost& host) override;

        /// @brief
        /// Purpose: Resets runtime-only animation state before the plugin detaches.
        /// @details
        /// Ownership: Does not own external resources.
        /// Thread Safety: Not thread-safe; call from the main thread during plugin detachment.

        void on_detach() override;

        /// @brief
        /// Purpose: Advances the example animation and updates entity transforms and colors.
        /// @details
        /// Ownership: Uses host-owned entities and components without taking ownership.
        /// Thread Safety: Not thread-safe; call from the main thread once per frame.

        void on_update(const tbx::DeltaTime& dt) override;

      private:
        float _elapsed_seconds = 0.0f;
    };
}
