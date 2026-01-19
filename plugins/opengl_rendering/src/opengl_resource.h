#pragma once
#include "tbx/tbx_api.h"
#include <utility>

namespace tbx::plugins::openglrendering
{
    /// <summary>Base interface for OpenGL resources that can be bound/unbound.</summary>
    /// <remarks>Purpose: Provides a common contract for binding OpenGL state.
    /// Ownership: Implementations own their underlying OpenGL handles.
    /// Thread Safety: Not thread-safe; use on the render thread.</remarks>
    class IGlResource
    {
      public:
        /// <summary>Destroys the OpenGL resource.</summary>
        /// <remarks>Purpose: Releases any owned OpenGL handles.
        /// Ownership: The resource owns its OpenGL state and releases it here.
        /// Thread Safety: Destroy on the render thread.</remarks>
        virtual ~IGlResource() = default;

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
        /// <summary>Binds the provided resource for the scope lifetime.</summary>
        /// <remarks>Purpose: Automates bind/unbind around a scope.
        /// Ownership: Non-owning reference; resource must outlive the scope.
        /// Thread Safety: Use only on the render thread.</remarks>
        explicit GlResourceScope(IGlResource& resource)
            : _resource(&resource)
        {
            _resource->bind();
        }

        /// <summary>Unbinds the resource when the scope ends.</summary>
        /// <remarks>Purpose: Ensures resource state is released.
        /// Ownership: Does not own the resource.
        /// Thread Safety: Use only on the render thread.</remarks>
        ~GlResourceScope()
        {
            if (_resource)
            {
                _resource->unbind();
            }
        }

        GlResourceScope(const GlResourceScope&) = delete;
        GlResourceScope& operator=(const GlResourceScope&) = delete;

        /// <summary>Moves the scope to transfer ownership of the binding.</summary>
        /// <remarks>Purpose: Allows scopes to be stored in containers.
        /// Ownership: Transfers non-owning pointer ownership.
        /// Thread Safety: Use only on the render thread.</remarks>
        GlResourceScope(GlResourceScope&& other) noexcept
            : _resource(std::exchange(other._resource, nullptr))
        {
        }

        /// <summary>Destroys the current binding and adopts another scope.</summary>
        /// <remarks>Purpose: Moves binding ownership between scopes.
        /// Ownership: Transfers non-owning pointer ownership.
        /// Thread Safety: Use only on the render thread.</remarks>
        GlResourceScope& operator=(GlResourceScope&& other) noexcept
        {
            if (this != &other)
            {
                if (_resource)
                {
                    _resource->unbind();
                }
                _resource = std::exchange(other._resource, nullptr);
            }
            return *this;
        }

      private:
        IGlResource* _resource = nullptr;
    };
}
