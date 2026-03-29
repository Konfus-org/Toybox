#pragma once

namespace opengl_rendering
{
    /// @brief
    /// Purpose: Provides a common contract for binding OpenGL state.
    /// @details
    /// Ownership: Implementations own their underlying OpenGL handles.
    /// Thread Safety: Not thread-safe; use on the render thread.
    class IOpenGlResource
    {
      public:
        virtual ~IOpenGlResource() noexcept = default;

        /// @brief
        /// Purpose: Makes the resource active for subsequent draw calls.
        /// @details
        /// Ownership: Does not transfer ownership of any handles.
        /// Thread Safety: Call only on the render thread.
        virtual void bind() = 0;

        /// @brief
        /// Purpose: Clears the resource binding.
        /// @details
        /// Ownership: Does not transfer ownership of any handles.
        /// Thread Safety: Call only on the render thread.
        virtual void unbind() = 0;
    };

    /// @brief
    /// Purpose: Ensures OpenGL resources are unbound when leaving scope.
    /// @details
    /// Ownership: Does not take ownership of the resource; stores a non-owning pointer.
    /// Thread Safety: Use only on the render thread.
    class OpenGlResourceScope final
    {
      public:
        OpenGlResourceScope(IOpenGlResource& resource);
        OpenGlResourceScope(const OpenGlResourceScope&) = delete;
        OpenGlResourceScope(OpenGlResourceScope&& other) noexcept;
        ~OpenGlResourceScope() noexcept;

        OpenGlResourceScope& operator=(const OpenGlResourceScope&) = delete;
        OpenGlResourceScope& operator=(OpenGlResourceScope&& other) noexcept;

      private:
        IOpenGlResource* _resource = nullptr;
    };
}
