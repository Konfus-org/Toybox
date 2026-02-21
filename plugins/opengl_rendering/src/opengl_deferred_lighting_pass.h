#pragma once
#include "opengl_render_pipeline.h"

namespace tbx::plugins
{
    class OpenGlResourceManager;

    /// <summary>
    /// Purpose: Executes the deferred-lighting composition pass between geometry and post-process.
    /// </summary>
    /// <remarks>
    /// Ownership: Stateless pass object; does not own GPU resources.
    /// Thread Safety: Not thread-safe; execute on the render thread.
    /// </remarks>
    class OpenGlDeferredLightingPass final
    {
      public:
        /// <summary>
        /// Purpose: Composes lit scene output from the geometry target into the lighting target.
        /// </summary>
        /// <remarks>
        /// Ownership: Uses non-owning frame buffer pointers from frame context.
        /// Thread Safety: Call only on the render thread with a current OpenGL context.
        /// </remarks>
        void execute(
            const OpenGlRenderFrameContext& frame_context,
            OpenGlResourceManager& resource_manager) const;
    };
}
