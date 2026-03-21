#pragma once
#include "opengl_resource.h"
#include "tbx/common/int.h"
#include "tbx/graphics/graphics_settings.h"
#include "tbx/graphics/mesh.h"
#include "tbx/graphics/vertex.h"
#include "tbx/math/size.h"
#include <glad/glad.h>

namespace opengl_rendering
{
    /// <summary>OpenGL vertex buffer resource.</summary>
    /// <remarks>Purpose: Owns vertex data uploaded to GPU memory and configures VAO attribute
    /// state. Ownership: Owns the OpenGL buffer handle and never owns caller-provided CPU data.
    /// Thread Safety: Not thread-safe; call from the render thread.</remarks>
    class OpenGlVertexBuffer final : public IOpenGlResource
    {
      public:
        OpenGlVertexBuffer();
        OpenGlVertexBuffer(const OpenGlVertexBuffer&) = delete;
        OpenGlVertexBuffer& operator=(const OpenGlVertexBuffer&) = delete;
        OpenGlVertexBuffer(OpenGlVertexBuffer&& other) noexcept;
        OpenGlVertexBuffer& operator=(OpenGlVertexBuffer&& other) noexcept;
        ~OpenGlVertexBuffer() noexcept override;

        void upload(tbx::uint32 vertex_array_id, const tbx::VertexBuffer& buffer);
        void bind() override;
        void unbind() override;

        tbx::uint32 get_count() const;

      private:
        tbx::uint32 _buffer_id = 0;
        tbx::uint32 _count = 0;
    };

    /// <summary>OpenGL index buffer resource.</summary>
    /// <remarks>Purpose: Owns index data uploaded to GPU memory and binds it to a VAO.
    /// Ownership: Owns the OpenGL buffer handle and never owns caller-provided CPU data.
    /// Thread Safety: Not thread-safe; call from the render thread.</remarks>
    class OpenGlIndexBuffer final : public IOpenGlResource
    {
      public:
        OpenGlIndexBuffer();
        OpenGlIndexBuffer(const OpenGlIndexBuffer&) = delete;
        OpenGlIndexBuffer& operator=(const OpenGlIndexBuffer&) = delete;
        OpenGlIndexBuffer(OpenGlIndexBuffer&& other) noexcept;
        OpenGlIndexBuffer& operator=(OpenGlIndexBuffer&& other) noexcept;
        ~OpenGlIndexBuffer() noexcept override;

        void upload(tbx::uint32 vertex_array_id, const tbx::IndexBuffer& buffer);
        void bind() override;
        void unbind() override;

        tbx::uint32 get_count() const;

      private:
        tbx::uint32 _buffer_id = 0;
        tbx::uint32 _count = 0;
    };

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
        /// Purpose: Prepares the g-buffer framebuffer for geometry rendering.
        /// Ownership: Does not transfer framebuffer ownership.
        /// Thread Safety: Not thread-safe; render-thread only.
        /// </summary>
        void prepare_geometry_pass() const;

        /// <summary>
        /// Purpose: Copies the selected render stage texture to the default framebuffer.
        /// Ownership: Does not transfer ownership of any OpenGL object.
        /// Thread Safety: Not thread-safe; render-thread only.
        /// </summary>
        void present(tbx::RenderStage render_stage, const tbx::Size& viewport_size) const;

        /// <summary>
        /// Purpose: Prepares the g-buffer framebuffer for passes that only write final color.
        /// Ownership: Does not transfer ownership of any OpenGL object.
        /// Thread Safety: Not thread-safe; render-thread only.
        /// </summary>
        void bind() override;

        /// <summary>
        /// Purpose: Restores rendering to the default framebuffer.
        /// Ownership: Does not transfer ownership of any OpenGL object.
        /// Thread Safety: Not thread-safe; render-thread only.
        /// </summary>
        void unbind() override;

        /// <summary>
        /// Purpose: Returns the albedo texture used by deferred lighting.
        /// Ownership: Returns a non-owning OpenGL texture handle.
        /// Thread Safety: Not thread-safe; render-thread only.
        /// </summary>
        GLuint get_albedo_texture() const;

        /// <summary>
        /// Purpose: Returns the normal texture used by deferred lighting.
        /// Ownership: Returns a non-owning OpenGL texture handle.
        /// Thread Safety: Not thread-safe; render-thread only.
        /// </summary>
        GLuint get_normal_texture() const;

        /// <summary>
        /// Purpose: Returns the emissive texture used by deferred lighting.
        /// Ownership: Returns a non-owning OpenGL texture handle.
        /// Thread Safety: Not thread-safe; render-thread only.
        /// </summary>
        GLuint get_emissive_texture() const;

        /// <summary>
        /// Purpose: Returns the packed material-properties texture used by deferred lighting.
        /// Ownership: Returns a non-owning OpenGL texture handle.
        /// Thread Safety: Not thread-safe; render-thread only.
        /// </summary>
        GLuint get_material_texture() const;

        /// <summary>
        /// Purpose: Returns the depth texture used for position reconstruction.
        /// Ownership: Returns a non-owning OpenGL texture handle.
        /// Thread Safety: Not thread-safe; render-thread only.
        /// </summary>
        GLuint get_depth_texture() const;

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
        GLuint _emissive = 0U;
        GLuint _material = 0U;
        GLuint _depth = 0U;
    };
}
