#pragma once
#include "opengl_render_pipeline.h"

namespace tbx::plugins
{
    class OpenGlResourceManager;

    /// <summary>
    /// Purpose: Clears the geometry target and renders optional sky content for the frame.
    /// </summary>
    /// <remarks>
    /// Ownership: Stateless pass object; does not own GPU resources.
    /// Thread Safety: Not thread-safe; execute on the render thread.
    /// </remarks>
    class OpenGlSkyPass final
    {
      public:
        /// <summary>
        /// Purpose: Clears the geometry framebuffer and draws the configured sky material when
        /// available.
        /// </summary>
        /// <remarks>
        /// Ownership: Uses non-owning frame buffer pointers and resource manager references.
        /// Thread Safety: Call only on the render thread with a current OpenGL context.
        /// </remarks>
        void execute(
            const OpenGlRenderFrameContext& frame_context,
            OpenGlResourceManager& resource_manager) const;
    };
}
