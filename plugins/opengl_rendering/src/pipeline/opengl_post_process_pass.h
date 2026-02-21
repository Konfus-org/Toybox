#pragma once
#include "opengl_render_pipeline.h"

namespace tbx::plugins
{
    class OpenGlResourceManager;

    /// <summary>
    /// Purpose: Executes stacked fullscreen post-processing passes and presents the final frame.
    /// </summary>
    /// <remarks>
    /// Ownership: Stateless pass object; consumes non-owning frame/context resources.
    /// Thread Safety: Not thread-safe; execute on the render thread.
    /// </remarks>
    class OpenGlPostProcessPass final
    {
      public:
        /// <summary>
        /// Purpose: Runs every enabled post-process effect in order and outputs to the present
        /// target.
        /// </summary>
        /// <remarks>
        /// Ownership: Uses non-owning frame buffers and shared draw resources managed elsewhere.
        /// Thread Safety: Call only on the render thread with a current OpenGL context.
        /// </remarks>
        void execute(
            const OpenGlRenderFrameContext& frame_context,
            OpenGlResourceManager& resource_manager) const;
    };
}
