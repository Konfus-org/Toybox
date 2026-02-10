#pragma once
#include "opengl_render_pipeline.h"
#include "opengl_resources/opengl_buffers.h"
#include "tbx/common/uuid.h"
#include "tbx/graphics/messages.h"
#include "tbx/plugin_api/plugin.h"

namespace tbx::plugins
{
    /// <summary>Hosts the OpenGL rendering backend implementation.</summary>
    /// <remarks>Purpose: Owns OpenGL pipeline orchestration and frame submission.
    /// Ownership: Owns render pipeline and framebuffer resources.
    /// Thread Safety: Not thread-safe; use on the render thread.</remarks>
    class OpenGlRenderingPlugin final : public Plugin
    {
      public:
        void on_attach(IPluginHost& host) override;
        void on_detach() override;
        void on_update(const DeltaTime& dt) override;
        void on_recieve_message(Message& msg) override;

      private:
        /// <summary>Initializes OpenGL state and debug output.</summary>
        /// <remarks>Purpose: Configures core global render state once context is available.
        /// Ownership: Does not transfer ownership.
        /// Thread Safety: Call on the render thread with active OpenGL context.</remarks>
        void initialize_opengl() const;

        /// <summary>Updates the window viewport size used for presentation.</summary>
        /// <remarks>Purpose: Tracks output viewport dimensions for final blit operations.
        /// Ownership: Stores the provided value inside plugin-owned state.
        /// Thread Safety: Call on the render thread.</remarks>
        void set_viewport_size(const Size& viewport_size);

        /// <summary>Updates the internal render resolution and framebuffer size.</summary>
        /// <remarks>Purpose: Controls rasterization resolution independently from viewport size.
        /// Ownership: Resizes plugin-owned framebuffer attachments.
        /// Thread Safety: Call on the render thread.</remarks>
        void set_render_resolution(const Size& render_resolution);

      private:
        Uuid _window_id = invalid::uuid;
        Size _viewport_size = {0, 0};
        Size _render_resolution = {0, 0};
        bool _is_context_ready = false;
        OpenGlFrameBuffer _framebuffer = {};
        OpenGlRenderPipeline _render_pipeline = {};
    };
}
