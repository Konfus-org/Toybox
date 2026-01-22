#pragma once
#include "opengl_resource.h"
#include "tbx/common/int.h"
#include "tbx/graphics/texture.h"

namespace tbx::plugins
{
    /// <summary>OpenGL implementation of a texture resource.</summary>
    /// <remarks>Purpose: Wraps an OpenGL texture object and its binding state.
    /// Ownership: Owns the OpenGL texture identifier.
    /// Thread Safety: Not thread-safe; use on the render thread.</remarks>
    class OpenGlTexture final : public IGlResource
    {
      public:
        /// <summary>Creates an OpenGL texture from CPU texture data.</summary>
        /// <remarks>Purpose: Uploads texture data into a GPU texture object.
        /// Ownership: Owns the created GPU texture.
        /// Thread Safety: Construct on the render thread.</remarks>
        explicit OpenGlTexture(const Texture& texture);

        /// <summary>Destroys the OpenGL texture resource.</summary>
        /// <remarks>Purpose: Releases the GPU texture handle.
        /// Ownership: Owns the GPU handle being destroyed.
        /// Thread Safety: Destroy on the render thread.</remarks>
        ~OpenGlTexture() noexcept override;

        /// <summary>Sets the texture unit slot used for activation.</summary>
        /// <remarks>Purpose: Configures the target texture unit.
        /// Ownership: Does not transfer ownership.
        /// Thread Safety: Call only on the render thread.</remarks>
        void set_slot(uint32 slot);

        /// <summary>Binds the texture to its configured unit.</summary>
        /// <remarks>Purpose: Makes the texture active for sampling.
        /// Ownership: The resource retains ownership of its GPU handle.
        /// Thread Safety: Call only on the render thread.</remarks>
        void bind() override;

        /// <summary>Unbinds the texture from its configured unit.</summary>
        /// <remarks>Purpose: Clears the texture binding for the unit.
        /// Ownership: The resource retains ownership of its GPU handle.
        /// Thread Safety: Call only on the render thread.</remarks>
        void unbind() override;

      private:
        uint32 _texture_id = 0;
        uint32 _slot = 0;
    };
}
