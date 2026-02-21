#pragma once
#include "tbx/tbx_api.h"

namespace tbx::plugins
{
    /// <summary>Base interface for OpenGL resources that can be bound/unbound.</summary>
    /// <remarks>Purpose: Provides a common contract for binding OpenGL state.
    /// Ownership: Implementations own their underlying OpenGL handles.
    /// Thread Safety: Not thread-safe; use on the render thread.</remarks>
    class IOpenGlResource
    {
      public:
        /// <summary>Destroys the OpenGL resource.</summary>
        /// <remarks>Purpose: Releases any owned OpenGL handles.
        /// Ownership: The resource owns its OpenGL state and releases it here.
        /// Thread Safety: Destroy on the render thread.</remarks>
        virtual ~IOpenGlResource() noexcept = default;

        /// <summary>Binds the OpenGL resource.</summary>
        /// <remarks>Purpose: Makes the resource active for subsequent draw calls.
        /// Ownership: Does not transfer ownership of any handles.
        /// Thread Safety: Call only on the render thread.</remarks>
        virtual void bind() = 0;

        /// <summary>Unbinds the OpenGL resource.</summary>
        /// <remarks>Purpose: Clears the resource binding.
        /// Ownership: Does not transfer ownership of any handles.
        /// Thread Safety: Call only on the render thread.</remarks>
        virtual void unbind() = 0;
    };

    /// <summary>RAII scope that binds a resource and unbinds on destruction.</summary>
    /// <remarks>Purpose: Ensures OpenGL resources are unbound when leaving scope.
    /// Ownership: Does not take ownership of the resource; stores a non-owning pointer.
    /// Thread Safety: Use only on the render thread.</remarks>
    class GlResourceScope final
    {
      public:
        GlResourceScope(IOpenGlResource& resource);
        GlResourceScope(const GlResourceScope&) = delete;
        GlResourceScope(GlResourceScope&& other) noexcept;
        ~GlResourceScope() noexcept;

        GlResourceScope& operator=(const GlResourceScope&) = delete;
        GlResourceScope& operator=(GlResourceScope&& other) noexcept;

      private:
        IOpenGlResource* _resource = nullptr;
    };
}
