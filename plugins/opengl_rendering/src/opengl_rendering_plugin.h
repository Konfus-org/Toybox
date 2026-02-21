#pragma once
#include "opengl_renderer.h"
#include "tbx/common/uuid.h"
#include "tbx/plugin_api/plugin.h"
#include <memory>

namespace tbx::plugins
{
    /// <summary>Hosts the OpenGL rendering backend implementation.</summary>
    /// <remarks>Purpose: Owns OpenGL renderer lifetime and routes window events.
    /// Ownership: Owns one active renderer and active target window id.
    /// Thread Safety: Not thread-safe; use on the render thread.</remarks>
    class OpenGlRenderingPlugin final : public Plugin
    {
      public:
        void on_attach(IPluginHost& host) override;
        void on_detach() override;
        void on_update(const DeltaTime& dt) override;
        void on_recieve_message(Message& msg) override;

      private:
        /// <summary>Tears down the active OpenGL renderer.</summary>
        /// <remarks>Purpose: Ensures renderer resources are released on detach.
        /// Ownership: Releases plugin-owned renderer.
        /// Thread Safety: Call on render thread.</remarks>
        void teardown_renderer();

      private:
        std::unique_ptr<OpenGlRenderer> _open_gl_renderer = nullptr;
        Uuid _target_window_id = Uuid::NONE;
    };
}
