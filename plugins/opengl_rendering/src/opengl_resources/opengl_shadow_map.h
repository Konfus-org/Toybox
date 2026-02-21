#pragma once
#include "opengl_resource.h"
#include "opengl_texture.h"
#include "tbx/common/int.h"
#include "tbx/math/size.h"
#include <memory>

namespace tbx::plugins
{
    /// <summary>Owns one OpenGL shadow-map depth texture resource.</summary>
    /// <remarks>Purpose: Encapsulates shadow depth texture allocation and lifetime.
    /// Ownership: Owns the runtime depth texture object.
    /// Thread Safety: Not thread-safe; use on the render thread.</remarks>
    class OpenGlShadowMap final : public IOpenGlResource
    {
      public:
        /// <summary>Creates an empty shadow-map resource.</summary>
        /// <remarks>Purpose: Defers texture allocation until set_resolution().
        /// Ownership: Owns no GPU texture until initialized.
        /// Thread Safety: Construct on the render thread.</remarks>
        OpenGlShadowMap() = default;

        /// <summary>Creates and allocates a shadow-map depth texture.</summary>
        /// <remarks>Purpose: Allocates depth texture storage for shadow rendering.
        /// Ownership: Owns the created depth texture.
        /// Thread Safety: Construct on the render thread.</remarks>
        explicit OpenGlShadowMap(const Size& resolution);

        OpenGlShadowMap(const OpenGlShadowMap&) = delete;
        OpenGlShadowMap& operator=(const OpenGlShadowMap&) = delete;

        /// <summary>Destroys the shadow-map resource and owned texture.</summary>
        /// <remarks>Purpose: Releases owned depth texture storage.
        /// Ownership: Releases owned runtime texture.
        /// Thread Safety: Destroy on the render thread.</remarks>
        ~OpenGlShadowMap() noexcept override;

        /// <summary>Binds the shadow-map texture resource.</summary>
        /// <remarks>Purpose: Satisfies IOpenGlResource interface for scoped usage.
        /// Ownership: Does not transfer ownership.
        /// Thread Safety: Call on the render thread.</remarks>
        void bind() override;

        /// <summary>Unbinds the shadow-map texture resource.</summary>
        /// <remarks>Purpose: Satisfies IOpenGlResource interface for scoped usage.
        /// Ownership: Does not transfer ownership.
        /// Thread Safety: Call on the render thread.</remarks>
        void unbind() override;

        /// <summary>Updates depth texture resolution.</summary>
        /// <remarks>Purpose: Recreates the underlying shadow depth texture.
        /// Ownership: Replaces and owns the new runtime texture.
        /// Thread Safety: Call on the render thread.</remarks>
        void set_resolution(const Size& resolution);

        /// <summary>Returns the current shadow-map resolution.</summary>
        /// <remarks>Purpose: Exposes allocated shadow-map size.
        /// Ownership: Returns value; no ownership transfer.
        /// Thread Safety: Safe on render thread.</remarks>
        Size get_resolution() const;

        /// <summary>Returns the OpenGL texture identifier for this shadow map.</summary>
        /// <remarks>Purpose: Allows direct attachment/bind calls in shadow passes.
        /// Ownership: Returns value; no ownership transfer.
        /// Thread Safety: Safe on render thread.</remarks>
        uint32 get_texture_id() const;

        /// <summary>Returns shared ownership of the shadow depth texture wrapper.</summary>
        /// <remarks>Purpose: Exposes wrapped texture to internal renderer systems.
        /// Ownership: Returns shared ownership.
        /// Thread Safety: Not thread-safe; use on render thread.</remarks>
        std::shared_ptr<OpenGlTexture> get_texture() const;

      private:
        std::shared_ptr<OpenGlTexture> _texture = nullptr;
        Size _resolution = {};
    };
}
