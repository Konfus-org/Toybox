#pragma once
#include "opengl_render_pipeline.h"

namespace tbx::plugins
{
    class OpenGlResourceManager;

    /// <summary>
    /// Purpose: Accumulates local lights through instanced proxy-volume rendering.
    /// </summary>
    /// <remarks>
    /// Ownership: Stateless pass object; consumes non-owning frame/context resources.
    /// Thread Safety: Not thread-safe; execute on the render thread.
    /// </remarks>
    class OpenGlDeferredLocalLightVolumePass final
    {
      public:
        /// <summary>
        /// Purpose: Renders additive point/spot/area light contributions using instanced proxies.
        /// </summary>
        /// <remarks>
        /// Ownership: Uses non-owning shader, framebuffer, and SSBO handles from frame context.
        /// Thread Safety: Call only on the render thread with a current OpenGL context.
        /// </remarks>
        void execute(
            const OpenGlRenderFrameContext& frame_context,
            OpenGlResourceManager& resource_manager) const;
    };
}
