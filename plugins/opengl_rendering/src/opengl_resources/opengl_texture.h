#pragma once
#include "opengl_resource.h"
#include "tbx/common/typedefs.h"
#include "tbx/graphics/texture.h"
#include "tbx/math/size.h"
#include "tbx/math/vectors.h"

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
        OpenGlTexture(const OpenGlTexture&) = delete;
        OpenGlTexture& operator=(const OpenGlTexture&) = delete;
        OpenGlTexture(OpenGlTexture&& other) noexcept;
        OpenGlTexture& operator=(OpenGlTexture&& other) noexcept;
        ~OpenGlTexture() noexcept override;

        void set_slot(uint32 slot);

        void bind() override;
        void unbind() override;

        uint32 get_texture_id() const;
        uint64 get_bindless_handle() const;

      private:
        uint32 _texture_id = 0;
        uint32 _slot = 0;
        mutable uint64 _bindless_handle = 0;
    };
}
