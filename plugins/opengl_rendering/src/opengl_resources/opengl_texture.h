#pragma once
#include "opengl_resource.h"
#include "tbx/common/int.h"
#include "tbx/graphics/texture.h"
#include "tbx/math/size.h"
#include "tbx/math/vectors.h"

namespace opengl_rendering
{
    using namespace tbx;
    /// <summary>OpenGL implementation of a texture resource.</summary>
    /// <remarks>Purpose: Wraps an OpenGL texture object and its binding state.
    /// Ownership: Owns the OpenGL texture identifier.
    /// Thread Safety: Not thread-safe; use on the render thread.</remarks>
    class OpenGlTexture final : public IOpenGlResource
    {
      public:
        OpenGlTexture(const Texture& texture);
        ~OpenGlTexture() noexcept override;

        void set_slot(uint32 slot);

        void bind() override;
        void unbind() override;

        uint32 get_texture_id() const;

      private:
        uint32 _texture_id = 0;
        uint32 _slot = 0;
    };
}
