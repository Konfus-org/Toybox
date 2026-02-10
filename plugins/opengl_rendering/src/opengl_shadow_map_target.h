#pragma once
#include "tbx/common/int.h"
#include "tbx/math/size.h"

namespace tbx::plugins
{
    /// <summary>OpenGL depth-only render target used for shadow mapping.</summary>
    /// <remarks>
    /// Purpose: Owns a framebuffer and depth texture sized for shadow map rendering.
    /// Ownership: Owns OpenGL framebuffer and depth texture handles.
    /// Thread Safety: Not thread-safe; render-thread only.
    /// </remarks>
    class OpenGlShadowMapTarget final
    {
      public:
        /// <summary>Creates an empty shadow map target.</summary>
        /// <remarks>
        /// Purpose: Initializes handle storage without allocating GPU resources.
        /// Ownership: Owns no GPU resources until initialized.
        /// Thread Safety: Construct on the render thread.
        /// </remarks>
        OpenGlShadowMapTarget() = default;
        OpenGlShadowMapTarget(const OpenGlShadowMapTarget&) = delete;
        OpenGlShadowMapTarget& operator=(const OpenGlShadowMapTarget&) = delete;
        OpenGlShadowMapTarget(OpenGlShadowMapTarget&&) noexcept = default;
        OpenGlShadowMapTarget& operator=(OpenGlShadowMapTarget&&) noexcept = default;
        ~OpenGlShadowMapTarget() = default;

        /// <summary>Creates or resizes the depth-only framebuffer resources.</summary>
        /// <remarks>
        /// Purpose: Allocates a depth texture and attaches it to a framebuffer.
        /// Ownership: Owns any newly created framebuffer resources.
        /// Thread Safety: Call only on the render thread.
        /// </remarks>
        bool try_resize(const Size& size);

        /// <summary>Destroys the framebuffer resources.</summary>
        /// <remarks>
        /// Purpose: Releases the depth texture and framebuffer handles.
        /// Ownership: Owns and releases GPU handles.
        /// Thread Safety: Call only on the render thread.
        /// </remarks>
        void destroy();

        /// <summary>Returns the framebuffer handle.</summary>
        /// <remarks>
        /// Purpose: Allows binding the shadow target for drawing.
        /// Ownership: Returns a value; no ownership transfer.
        /// Thread Safety: Safe to call on the render thread.
        /// </remarks>
        uint32 get_framebuffer() const;

        /// <summary>Returns the depth texture handle.</summary>
        /// <remarks>
        /// Purpose: Allows binding the shadow map for sampling.
        /// Ownership: Returns a value; no ownership transfer.
        /// Thread Safety: Safe to call on the render thread.
        /// </remarks>
        uint32 get_depth_texture() const;

        /// <summary>Returns the current shadow map size.</summary>
        /// <remarks>
        /// Purpose: Enables callers to track allocated dimensions.
        /// Ownership: Returns a value; no ownership transfer.
        /// Thread Safety: Safe to call on the render thread.
        /// </remarks>
        Size get_size() const;

      private:
        uint32 _framebuffer = 0U;
        uint32 _depth_texture = 0U;
        Size _size = {0U, 0U};
    };
}

