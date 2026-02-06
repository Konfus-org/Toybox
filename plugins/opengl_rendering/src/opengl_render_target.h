#pragma once
#include "opengl_shader.h"
#include "tbx/common/int.h"
#include "tbx/math/size.h"
#include <memory>

namespace tbx::plugins
{
    /// <summary>Fullscreen presentation pipeline for a render target.</summary>
    /// <remarks>
    /// Purpose: Holds the shader program and VAO needed to present a render target texture.
    /// Ownership: Owns the VAO handle and shared program reference.
    /// Thread Safety: Not thread-safe; use on the render thread.
    /// </remarks>
    struct OpenGlPresentPipeline
    {
        std::shared_ptr<OpenGlShaderProgram> program = {};
        uint32 vertex_array = 0U;

        /// <summary>Returns whether the pipeline has a ready program and VAO.</summary>
        /// <remarks>
        /// Purpose: Allows callers to check if presentation resources are initialized.
        /// Ownership: Does not transfer ownership.
        /// Thread Safety: Safe to call on the render thread.
        /// </remarks>
        bool is_ready() const;

        /// <summary>Initializes the pipeline if it is not already ready.</summary>
        /// <remarks>
        /// Purpose: Builds the present shader program and VAO.
        /// Ownership: Owns the created VAO; shares program ownership.
        /// Thread Safety: Call only on the render thread.
        /// </remarks>
        bool try_initialize();

        /// <summary>Destroys GPU resources owned by the pipeline.</summary>
        /// <remarks>
        /// Purpose: Releases the VAO and shader program reference.
        /// Ownership: Releases owned GPU handles.
        /// Thread Safety: Call only on the render thread.
        /// </remarks>
        void destroy();
    };

    /// <summary>OpenGL render target with color and depth-stencil attachments.</summary>
    /// <remarks>
    /// Purpose: Owns framebuffer resources for offscreen rendering and presentation.
    /// Ownership: Owns OpenGL framebuffer, texture, and renderbuffer handles.
    /// Thread Safety: Not thread-safe; use on the render thread.
    /// </remarks>
    class OpenGlRenderTarget final
    {
      public:
        /// <summary>Creates an empty render target.</summary>
        /// <remarks>
        /// Purpose: Initializes handle storage without allocating GPU resources.
        /// Ownership: Owns no GPU resources until initialized.
        /// Thread Safety: Construct on the render thread.
        /// </remarks>
        OpenGlRenderTarget() = default;
        OpenGlRenderTarget(const OpenGlRenderTarget&) = delete;
        OpenGlRenderTarget& operator=(const OpenGlRenderTarget&) = delete;
        OpenGlRenderTarget(OpenGlRenderTarget&&) noexcept = default;
        OpenGlRenderTarget& operator=(OpenGlRenderTarget&&) noexcept = default;

        /// <summary>Destroys the render target container.</summary>
        /// <remarks>
        /// Purpose: Relies on explicit destroy calls for GPU handles.
        /// Ownership: Does not implicitly destroy GPU resources.
        /// Thread Safety: Destroy on the render thread.
        /// </remarks>
        ~OpenGlRenderTarget() = default;

        /// <summary>Creates or resizes the render target resources.</summary>
        /// <remarks>
        /// Purpose: Allocates framebuffer resources sized to the given dimensions.
        /// Ownership: Owns any newly created framebuffer resources.
        /// Thread Safety: Call only on the render thread.
        /// </remarks>
        bool try_resize(const Size& size);

        /// <summary>Destroys the framebuffer resources.</summary>
        /// <remarks>
        /// Purpose: Releases texture, renderbuffer, and framebuffer handles.
        /// Ownership: Owns and releases GPU handles.
        /// Thread Safety: Call only on the render thread.
        /// </remarks>
        void destroy();

        /// <summary>Destroys the present pipeline resources.</summary>
        /// <remarks>
        /// Purpose: Releases the presentation VAO and program references.
        /// Ownership: Owns the pipeline resources being destroyed.
        /// Thread Safety: Call only on the render thread.
        /// </remarks>
        void destroy_present_pipeline();

        /// <summary>Returns the framebuffer handle.</summary>
        /// <remarks>
        /// Purpose: Allows binding the render target for drawing.
        /// Ownership: Returns a value; no ownership transfer.
        /// Thread Safety: Safe to call on the render thread.
        /// </remarks>
        uint32 get_framebuffer() const;

        /// <summary>Returns the color texture handle.</summary>
        /// <remarks>
        /// Purpose: Allows sampling the render target in presentation passes.
        /// Ownership: Returns a value; no ownership transfer.
        /// Thread Safety: Safe to call on the render thread.
        /// </remarks>
        uint32 get_color_texture() const;

        /// <summary>Returns the current render target size.</summary>
        /// <remarks>
        /// Purpose: Enables callers to track allocated dimensions.
        /// Ownership: Returns a value; no ownership transfer.
        /// Thread Safety: Safe to call on the render thread.
        /// </remarks>
        Size get_size() const;

        /// <summary>Returns the presentation pipeline state.</summary>
        /// <remarks>
        /// Purpose: Gives access to the present pipeline for rendering.
        /// Ownership: Returns a reference to owned pipeline state.
        /// Thread Safety: Call only on the render thread.
        /// </remarks>
        OpenGlPresentPipeline& get_present_pipeline();

        /// <summary>Returns the presentation pipeline state.</summary>
        /// <remarks>
        /// Purpose: Gives access to the present pipeline for rendering.
        /// Ownership: Returns a reference to owned pipeline state.
        /// Thread Safety: Call only on the render thread.
        /// </remarks>
        const OpenGlPresentPipeline& get_present_pipeline() const;

      private:
        uint32 _framebuffer = 0U;
        uint32 _color_texture = 0U;
        uint32 _depth_stencil = 0U;
        Size _size = {0U, 0U};
        OpenGlPresentPipeline _present_pipeline = {};
    };
}
