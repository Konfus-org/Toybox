#pragma once
#include "opengl_render_pipeline.h"

namespace tbx::plugins
{
    class OpenGlResourceManager;

    /// <summary>
    /// Purpose: Renders scene depth from light-space views into configured shadow-map textures.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns a reusable shadow framebuffer object for the pass lifetime.
    /// Thread Safety: Not thread-safe; execute and destroy on the render thread.
    /// </remarks>
    class OpenGlShadowPass final
    {
      public:
        OpenGlShadowPass() = default;
        ~OpenGlShadowPass() noexcept;
        /// <summary>
        /// Purpose: Populates each configured shadow-map texture using visible shadow-casting
        /// entities.
        /// </summary>
        /// <remarks>
        /// Ownership: Uses non-owning frame-context spans and resource manager references while
        /// writing to the owned pass framebuffer.
        /// Thread Safety: Call only on the render thread with a current OpenGL context.
        /// </remarks>
        void execute(
            const OpenGlRenderFrameContext& frame_context,
            OpenGlResourceManager& resource_manager);

      private:
        /// <summary>
        /// Purpose: Lazily creates a framebuffer used for all shadow map renders.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns the pass-owned framebuffer handle; caller does not own it.
        /// Thread Safety: Render-thread only because it touches OpenGL state.
        /// </remarks>
        uint32 get_or_create_shadow_framebuffer_id();

      private:
        uint32 _shadow_framebuffer_id = 0;
    };
}
