#pragma once
#include "opengl_resource.h"
#include "tbx/common/int.h"
#include "tbx/graphics/texture.h"
#include "tbx/math/size.h"
#include "tbx/math/vectors.h"

namespace opengl_rendering
{
    /// <summary>OpenGL implementation of a texture resource.</summary>
    /// <remarks>Purpose: Wraps an OpenGL texture object and its binding state.
    /// Ownership: Owns the OpenGL texture identifier.
    /// Thread Safety: Not thread-safe; use on the render thread.</remarks>
    class OpenGlTexture final : public IOpenGlResource
    {
      public:
        OpenGlTexture(const tbx::Texture& texture);
        OpenGlTexture(const OpenGlTexture&) = delete;
        OpenGlTexture& operator=(const OpenGlTexture&) = delete;
        OpenGlTexture(OpenGlTexture&& other) noexcept;
        OpenGlTexture& operator=(OpenGlTexture&& other) noexcept;
        ~OpenGlTexture() noexcept override;

        void set_slot(tbx::uint32 slot);

        void bind() override;
        void unbind() override;

        tbx::uint32 get_texture_id() const;
        tbx::uint64 get_bindless_handle() const;

      private:
        tbx::uint32 _texture_id = 0;
        tbx::uint32 _slot = 0;
        mutable tbx::uint64 _bindless_handle = 0;
    };
}
