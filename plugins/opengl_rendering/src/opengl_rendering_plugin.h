#pragma once
#include "opengl_renderer.h"
#include "tbx/common/uuid.h"
#include "tbx/plugin_api/plugin.h"
#include <memory>
#include <unordered_map>

namespace tbx::plugins
{
    /// <summary>Hosts the OpenGL rendering backend implementation.</summary>
    /// <remarks>Purpose: Owns OpenGL renderer lifetime and routes window events.
    /// Ownership: Owns one renderer per active OpenGL window.
    /// Thread Safety: Not thread-safe; use on the render thread.</remarks>
    class OpenGlRenderingPlugin final : public Plugin
    {
      public:
        void on_attach(IPluginHost& host) override;
        void on_detach() override;
        void on_update(const DeltaTime& dt) override;
        void on_recieve_message(Message& msg) override;

      private:
        /// <summary>Tears down one OpenGL renderer bound to a specific window.</summary>
        /// <remarks>Purpose: Ensures renderer resources are released when a window closes.
        /// Ownership: Releases plugin-owned renderer for the provided window id.
        /// Thread Safety: Call on render thread.</remarks>
        void teardown_renderer(const Uuid& window_id);

        /// <summary>Applies current shadow settings to all active renderer instances.</summary>
        /// <remarks>Purpose: Keeps per-window renderer shadow behavior aligned with app graphics
        /// settings. Ownership: Uses plugin-owned renderer instances; does not transfer ownership.
        /// Thread Safety: Call on render thread.</remarks>
        void apply_shadow_settings_to_renderers();

      private:
        std::unordered_map<Uuid, std::unique_ptr<OpenGlRenderer>> _renderers = {};
        OpenGlShadowSettings _shadow_settings = {};
    };
}
