#pragma once
#include "opengl_resource.h"
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/systems/graphics/texture.h"
#include "tbx/types/typedefs.h"

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
        OpenGlTexture(const tbx::Texture& texture);
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
        uint64 get_bindless_handle() const;
        void update(
            const tbx::GraphicsTextureUpdateDesc& desc,
            tbx::GraphicsTextureFormat format,
            const void* data) const;

      private:
        uint32 _texture_id = 0;
        mutable uint64 _bindless_handle = 0;
    };
}
