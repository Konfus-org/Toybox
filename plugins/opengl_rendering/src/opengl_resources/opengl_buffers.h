#pragma once
#include "opengl_resource.h"
#include "tbx/common/typedefs.h"
#include "tbx/graphics/mesh.h"
#include "tbx/graphics/vertex.h"
#include "tbx/math/size.h"
#include <glad/glad.h>

namespace opengl_rendering
{
    /// @brief
    /// Purpose: OpenGL vertex buffer resource.
    /// @details
    /// Ownership: Owns vertex data uploaded to GPU memory, configures VAO attribute state, and
    /// owns the OpenGL buffer handle without owning caller-provided CPU data.
    /// Thread Safety: Not thread-safe; call from the render thread.
    class OpenGlVertexBuffer final : public IOpenGlResource
    {
      public:
        OpenGlVertexBuffer();
        OpenGlVertexBuffer(const OpenGlVertexBuffer&) = delete;
        OpenGlVertexBuffer& operator=(const OpenGlVertexBuffer&) = delete;
        OpenGlVertexBuffer(OpenGlVertexBuffer&& other) noexcept;
        OpenGlVertexBuffer& operator=(OpenGlVertexBuffer&& other) noexcept;
        ~OpenGlVertexBuffer() noexcept override;

        void upload(uint32 vertex_array_id, const tbx::VertexBuffer& buffer);
        void bind() override;
        void unbind() override;

        uint32 get_count() const;

      private:
        uint32 _buffer_id = 0;
        uint32 _count = 0;
    };

    /// @brief
    /// Purpose: Owns index data uploaded to GPU memory and binds it to a VAO.
    /// @details
    /// Ownership: Owns the OpenGL buffer handle and never owns caller-provided CPU data.
    /// Thread Safety: Not thread-safe; call from the render thread.
    class OpenGlIndexBuffer final : public IOpenGlResource
    {
      public:
        OpenGlIndexBuffer();
        OpenGlIndexBuffer(const OpenGlIndexBuffer&) = delete;
        OpenGlIndexBuffer& operator=(const OpenGlIndexBuffer&) = delete;
        OpenGlIndexBuffer(OpenGlIndexBuffer&& other) noexcept;
        OpenGlIndexBuffer& operator=(OpenGlIndexBuffer&& other) noexcept;
        ~OpenGlIndexBuffer() noexcept override;

        void upload(uint32 vertex_array_id, const tbx::IndexBuffer& buffer);
        void bind() override;
        void unbind() override;

        uint32 get_count() const;

      private:
        uint32 _buffer_id = 0;
        uint32 _count = 0;
    };

    /// @brief
    /// Purpose: Owns deferred-rendering attachments used by future multi-pass rendering and debug
    /// presentation.
    /// @details
    /// Ownership: Owns OpenGL framebuffer and texture handles for the lifetime of this object.
    /// Thread Safety: Not thread-safe; use only on the active render thread/context.
    class OpenGlGBuffer final : public IOpenGlResource
    {
      public:
        OpenGlGBuffer() = default;
        OpenGlGBuffer(const OpenGlGBuffer&) = delete;
        OpenGlGBuffer& operator=(const OpenGlGBuffer&) = delete;
        ~OpenGlGBuffer() noexcept override;

        /// @brief
        /// Purpose: Recreates render targets to match the supplied dimensions.
        /// @details
        /// Ownership: Keeps ownership of all allocated OpenGL objects.
        /// Thread Safety: Not thread-safe; caller must synchronize access.
        void resize(const tbx::Size& size);

        /// @brief
        /// Purpose: Prepares the deferred framebuffer for geometry rendering.
        /// @details
        /// Ownership: Does not transfer framebuffer ownership.
        /// Thread Safety: Not thread-safe; render-thread only.
        void prepare_geometry_pass() const;

        /// @brief
        /// Purpose: Copies the final color texture to the default framebuffer.
        /// @details
        /// Ownership: Does not transfer ownership of any OpenGL object.
        /// Thread Safety: Not thread-safe; render-thread only.
        void present(const tbx::Size& viewport_size) const;

        /// @brief
        /// Purpose: Prepares the deferred framebuffer for passes that only write final color.
        /// @details
        /// Ownership: Does not transfer ownership of any OpenGL object.
        /// Thread Safety: Not thread-safe; render-thread only.
        void bind() override;

        /// @brief
        /// Purpose: Restores rendering to the default framebuffer.
        /// @details
        /// Ownership: Does not transfer ownership of any OpenGL object.
        /// Thread Safety: Not thread-safe; render-thread only.
        void unbind() override;

        /// @brief
        /// Purpose: Returns the albedo texture used by deferred lighting.
        /// @details
        /// Ownership: Returns a non-owning OpenGL texture handle.
        /// Thread Safety: Not thread-safe; render-thread only.
        GLuint get_albedo_texture() const;

        /// @brief
        /// Purpose: Returns the normal texture used by deferred lighting.
        /// @details
        /// Ownership: Returns a non-owning OpenGL texture handle.
        /// Thread Safety: Not thread-safe; render-thread only.
        GLuint get_normal_texture() const;

        /// @brief
        /// Purpose: Returns the emissive texture used by deferred lighting.
        /// @details
        /// Ownership: Returns a non-owning OpenGL texture handle.
        /// Thread Safety: Not thread-safe; render-thread only.
        GLuint get_emissive_texture() const;

        /// @brief
        /// Purpose: Returns the packed material-properties texture used by deferred lighting.
        /// @details
        /// Ownership: Returns a non-owning OpenGL texture handle.
        /// Thread Safety: Not thread-safe; render-thread only.
        GLuint get_material_texture() const;

        /// @brief
        /// Purpose: Returns the depth texture used for position reconstruction.
        /// @details
        /// Ownership: Returns a non-owning OpenGL texture handle.
        /// Thread Safety: Not thread-safe; render-thread only.
        GLuint get_depth_texture() const;

        /// @brief
        /// Purpose: Returns the resolved scene-color texture used by late render passes.
        /// @details
        /// Ownership: Returns a non-owning OpenGL texture handle.
        /// Thread Safety: Not thread-safe; render-thread only.
        GLuint get_final_color_texture() const;

        /// @brief
        /// Purpose: Applies an external color texture to the final-color target.
        /// @details
        /// Ownership: Does not transfer ownership of either OpenGL texture.
        /// Thread Safety: Not thread-safe; render-thread only.
        void apply_to_final_color(GLuint source_texture) const;

      private:
        static GLuint create_color_attachment(GLenum internal_format, uint32 width, uint32 height);
        static GLuint create_depth_attachment(uint32 width, uint32 height);
        static void delete_texture(GLuint& texture_id);
        void destroy();

      private:
        tbx::Size _size = {0U, 0U};
        GLuint _geometry_framebuffer = 0U;
        GLuint _final_color_framebuffer = 0U;
        GLuint _final_color = 0U;
        GLuint _geometry_preview_color = 0U;
        GLuint _albedo = 0U;
        GLuint _normal = 0U;
        GLuint _depth_preview = 0U;
        GLuint _emissive = 0U;
        GLuint _material = 0U;
        GLuint _depth = 0U;
    };
}
