#pragma once
#include "opengl_render_pipeline.h"

namespace opengl_rendering
{
    /// <summary>
    /// Purpose: Builds per-tile local-light index lists with a compute shader.
    /// </summary>
    /// <remarks>
    /// Ownership: Stateless pass object; consumes non-owning frame-context resources.
    /// Thread Safety: Not thread-safe; execute on the render thread.
    /// </remarks>
    class OpenGlLightCullingPass final
    {
      public:
        /// <summary>
        /// Purpose: Dispatches tiled light culling and populates SSBO tile headers/index data.
        /// </summary>
        /// <remarks>
        /// Ownership: Uses non-owning shader and SSBO handles from frame context.
        /// Thread Safety: Call only on the render thread with a current OpenGL context.
        /// </remarks>
        void execute(const OpenGlRenderFrameContext& frame_context) const;
    };
}
