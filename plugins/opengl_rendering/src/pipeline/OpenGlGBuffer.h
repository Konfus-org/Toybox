#pragma once
#include "opengl_resources/opengl_resource.h"
#include "tbx/graphics/graphics_settings.h"
#include "tbx/math/size.h"
#include <glad/glad.h>

namespace opengl_rendering
{
    /// <summary>
    /// Purpose: Owns geometry-buffer attachments used by future multi-pass rendering and debug
    /// presentation.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns OpenGL framebuffer and texture handles for the lifetime of this object.
    /// Thread Safety: Not thread-safe; use only on the active render thread/context.
    /// </remarks>
    class OpenGlGBuffer final : public IOpenGlResource
    {
      public:
        OpenGlGBuffer() = default;
        OpenGlGBuffer(const OpenGlGBuffer&) = delete;
        OpenGlGBuffer& operator=(const OpenGlGBuffer&) = delete;
        ~OpenGlGBuffer() noexcept override;

        /// <summary>
        /// Purpose: Recreates render targets to match the supplied dimensions.
        /// Ownership: Keeps ownership of all allocated OpenGL objects.
        /// Thread Safety: Not thread-safe; caller must synchronize access.
        /// </summary>
        void resize(const tbx::Size& size);

        /// <summary>
        /// Purpose: Binds the g-buffer framebuffer and configures all draw attachments for
        /// geometry.
        /// Ownership: Does not transfer framebuffer ownership.
        /// Thread Safety: Not thread-safe; render-thread only.
        /// </summary>
        void bind() override;

        /// <summary>
        /// Purpose: Unbinds the g-buffer framebuffer and restores default framebuffer binding.
        /// Ownership: Does not transfer framebuffer ownership.
        /// Thread Safety: Not thread-safe; render-thread only.
        /// </summary>
        void unbind() override;

        /// <summary>
        /// Purpose: Copies the selected render stage texture to the default framebuffer.
        /// Ownership: Does not transfer ownership of any OpenGL object.
        /// Thread Safety: Not thread-safe; render-thread only.
        /// </summary>
        void present(tbx::RenderStage render_stage, const tbx::Size& viewport_size) const;

      private:
        static GLuint create_color_attachment(
            GLenum internal_format,
            tbx::uint32 width,
            tbx::uint32 height);
        static GLuint create_depth_attachment(tbx::uint32 width, tbx::uint32 height);
        static void delete_texture(GLuint& texture_id);
        void destroy();

      private:
        tbx::Size _size = {0U, 0U};
        GLuint _framebuffer = 0U;
        GLuint _final_color = 0U;
        GLuint _geometry_color = 0U;
        GLuint _albedo = 0U;
        GLuint _normal = 0U;
        GLuint _depth_visual = 0U;
        GLuint _depth = 0U;
    };
}
