#pragma once
#include "opengl_render_pipeline.h"

namespace tbx::plugins
{
    class OpenGlResourceManager;

    /// <summary>
    /// Purpose: Renders scene depth from light-space views into configured shadow-map textures.
    /// </summary>
    /// <remarks>
    /// Ownership: Stateless pass object; does not own GPU resources.
    /// Thread Safety: Not thread-safe; execute on the render thread.
    /// </remarks>
    class OpenGlShadowPass final
    {
      public:
        /// <summary>
        /// Purpose: Populates each configured shadow-map texture using visible shadow-casting
        /// entities.
        /// </summary>
        /// <remarks>
        /// Ownership: Uses non-owning frame-context spans and resource manager references.
        /// Thread Safety: Call only on the render thread with a current OpenGL context.
        /// </remarks>
        void execute(
            const OpenGlRenderFrameContext& frame_context,
            OpenGlResourceManager& resource_manager) const;
    };
}
