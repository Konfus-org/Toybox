#pragma once
#include "opengl_resource.h"
#include "opengl_resource_manager.h"
#include <vector>

namespace opengl_rendering
{
    /// <summary>
    /// Purpose: Stores resolved OpenGL resources for one post-processing stack.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns a vector of draw-resource values for the stack.
    /// Thread Safety: Not thread-safe; use on the render thread.
    /// </remarks>
    class OpenGlPostProcessingStackResource final : public IOpenGlResource
    {
      public:
        /// <summary>
        /// Purpose: One resolved effect draw payload in the post-processing stack.
        /// Ownership: Owns draw resources and blend value by value.
        /// Thread Safety: Not thread-safe; mutate on render thread.
        /// </summary>
        struct Effect final
        {
            OpenGlDrawResources draw_resources = {};
            float blend = 1.0f;
        };

        /// <summary>
        /// Purpose: Binds the stack resource.
        /// Ownership: No transfer.
        /// Thread Safety: Render thread only.
        /// </summary>
        void bind() override {}

        /// <summary>
        /// Purpose: Unbinds the stack resource.
        /// Ownership: No transfer.
        /// Thread Safety: Render thread only.
        /// </summary>
        void unbind() override {}

        /// <summary>
        /// Purpose: Appends one resolved effect to the stack.
        /// Ownership: Copies the effect into owned storage.
        /// Thread Safety: Not thread-safe; mutate on render thread.
        /// </summary>
        void add_effect(const Effect& effect)
        {
            _effects.push_back(effect);
        }

        /// <summary>
        /// Purpose: Gets ordered resolved effects for rendering.
        /// Ownership: Returns a const view to owned storage.
        /// Thread Safety: Read-only on render thread.
        /// </summary>
        const std::vector<Effect>& get_effects() const
        {
            return _effects;
        }

      private:
        std::vector<Effect> _effects = {};
    };
}
