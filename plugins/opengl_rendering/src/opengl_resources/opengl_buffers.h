#pragma once
#include "opengl_resource.h"
#include "opengl_texture.h"
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/types/typedefs.h"
#include <glad/glad.h>

namespace opengl_rendering
{
    /// @brief
    /// Purpose: Owns a command-backend OpenGL buffer.
    /// @details
    /// Ownership: Owns one OpenGL buffer object and stores its backend-neutral description.
    /// Thread Safety: Not thread-safe; use on the active OpenGL context thread.
    class OpenGlGraphicsBuffer final : public IOpenGlResource
    {
      public:
        OpenGlGraphicsBuffer(
            const tbx::GraphicsBufferDesc& desc,
            const void* data,
            uint64 data_size);
        ~OpenGlGraphicsBuffer() noexcept override;

      public:
        OpenGlGraphicsBuffer(const OpenGlGraphicsBuffer&) = delete;
        OpenGlGraphicsBuffer(OpenGlGraphicsBuffer&& other) noexcept;
        OpenGlGraphicsBuffer& operator=(const OpenGlGraphicsBuffer&) = delete;
        OpenGlGraphicsBuffer& operator=(OpenGlGraphicsBuffer&& other) noexcept;

      public:
        void bind() override;
        void bind_slot(uint32 slot) const;
        GLuint get_buffer_id() const;
        void unbind() override;
        void update(const void* data, uint64 data_size, uint64 offset) const;

      private:
        GLuint _buffer_id = 0U;
        GLenum _target = GL_ARRAY_BUFFER;
    };

    /// @brief
    /// Purpose: Owns the transient framebuffer used for explicit render passes.
    /// @details
    /// Ownership: Owns one framebuffer object and borrows attached texture handles.
    /// Thread Safety: Not thread-safe; use on the active OpenGL context thread.
    class OpenGlFramebuffer final : public IOpenGlResource
    {
      public:
        OpenGlFramebuffer();
        ~OpenGlFramebuffer() noexcept override;

      public:
        OpenGlFramebuffer(const OpenGlFramebuffer&) = delete;
        OpenGlFramebuffer(OpenGlFramebuffer&& other) noexcept;
        OpenGlFramebuffer& operator=(const OpenGlFramebuffer&) = delete;
        OpenGlFramebuffer& operator=(OpenGlFramebuffer&& other) noexcept;

      public:
        void attach_color(uint32 index, const OpenGlTexture& texture) const;
        void attach_depth_stencil(
            const OpenGlTexture& texture,
            tbx::GraphicsTextureFormat format,
            int32 layer) const;
        void bind() override;
        bool is_complete() const;
        void set_draw_buffers(uint32 color_target_count) const;
        void unbind() override;

      private:
        GLuint _framebuffer_id = 0U;
    };
}
