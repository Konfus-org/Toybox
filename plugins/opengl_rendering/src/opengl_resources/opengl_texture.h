#pragma once
#include "opengl_resource.h"
#include "tbx/common/int.h"
#include "tbx/graphics/texture.h"
#include "tbx/math/size.h"
#include "tbx/math/vectors.h"

namespace tbx::plugins
{
    /// <summary>Describes runtime texture allocation profiles for OpenGL resources.</summary>
    /// <remarks>Purpose: Selects OpenGL internal-format behavior for GPU-only allocations.
    /// Ownership: Value semantics only.
    /// Thread Safety: Immutable enum safe to read concurrently.</remarks>
    enum class OpenGlTextureRuntimeMode
    {
        Color,
        HdrColor,
        Depth,
        DepthStencil
    };

    /// <summary>Defines allocation settings for a runtime-created OpenGL texture.</summary>
    /// <remarks>Purpose: Provides dimensions, filtering, wrapping, and format mode for GPU
    /// allocations without CPU pixel upload. Ownership: Value semantics only. Thread Safety:
    /// Safe to read concurrently after construction.</remarks>
    struct OpenGlTextureRuntimeSettings final
    {
        OpenGlTextureRuntimeMode mode = OpenGlTextureRuntimeMode::Color;
        Size resolution = {};
        TextureFilter filter = TextureFilter::LINEAR;
        TextureWrap wrap = TextureWrap::CLAMP_TO_EDGE;
        bool use_border_color = false;
        Vec4 border_color = Vec4(0.0f);
        bool use_compare_mode = false;
    };

    /// <summary>OpenGL implementation of a texture resource.</summary>
    /// <remarks>Purpose: Wraps an OpenGL texture object and its binding state.
    /// Ownership: Owns the OpenGL texture identifier.
    /// Thread Safety: Not thread-safe; use on the render thread.</remarks>
    class OpenGlTexture final : public IOpenGlResource
    {
      public:
        /// <summary>Creates an OpenGL texture from CPU texture data.</summary>
        /// <remarks>Purpose: Uploads texture data into a GPU texture object.
        /// Ownership: Owns the created GPU texture.
        /// Thread Safety: Construct on the render thread.</remarks>
        explicit OpenGlTexture(const Texture& texture);

        /// <summary>Creates an OpenGL texture from runtime GPU allocation settings.</summary>
        /// <remarks>Purpose: Allocates texture storage without uploading CPU pixel data.
        /// Ownership: Owns the created GPU texture.
        /// Thread Safety: Construct on the render thread.</remarks>
        explicit OpenGlTexture(const OpenGlTextureRuntimeSettings& settings);

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

        /// <summary>Returns the underlying OpenGL texture object identifier.</summary>
        /// <remarks>Purpose: Supports low-level OpenGL interop for attachment and bind calls.
        /// Ownership: Returns value; no ownership transfer.
        /// Thread Safety: Safe to read on the render thread.</remarks>
        uint32 get_texture_id() const;

      private:
        uint32 _texture_id = 0;
        uint32 _slot = 0;
    };
}
