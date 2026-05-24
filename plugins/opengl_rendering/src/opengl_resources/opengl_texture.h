#pragma once
#include "opengl_resource.h"
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/types/typedefs.h"
#include <glad/glad.h>

namespace opengl_rendering
{
    /// @brief
    /// Purpose: Wraps an OpenGL texture object and its binding state.
    /// @details
    /// Ownership: Owns the OpenGL texture identifier.
    /// Thread Safety: Not thread-safe; use on the render thread.
    class OpenGlTexture final : public IOpenGlResource
    {
      public:
        OpenGlTexture(const tbx::GraphicsTextureDesc& desc, const void* data);
        OpenGlTexture(const OpenGlTexture&) = delete;
        OpenGlTexture& operator=(const OpenGlTexture&) = delete;
        OpenGlTexture(OpenGlTexture&& other) noexcept;
        OpenGlTexture& operator=(OpenGlTexture&& other) noexcept;
        ~OpenGlTexture() noexcept override;

        void bind_slot(uint32 slot) const;

        void bind() override;
        void unbind() override;

        uint32 get_texture_id() const;
        uint32 get_array_layer_count() const;
        void update(
            const tbx::GraphicsTextureUpdateDesc& desc,
            GLenum upload_format,
            GLenum upload_type,
            const void* data) const;

      private:
        uint32 _texture_id = 0;
        uint32 _array_layer_count = 1U;
    };

    /// @brief
    /// Purpose: Stores a texture and the backend facts needed after upload.
    struct OpenGlTextureResource
    {
        OpenGlTexture texture;
        tbx::Size size = {};
        uint64 bytes_per_pixel = 0U;
        uint32 array_layer_count = 1U;
        GLenum depth_attachment = GL_DEPTH_ATTACHMENT;
        GLenum internal_format = GL_RGBA8;
        GLenum upload_format = GL_RGBA;
        GLenum upload_type = GL_UNSIGNED_BYTE;
        bool is_storage_capable = false;
    };
}
