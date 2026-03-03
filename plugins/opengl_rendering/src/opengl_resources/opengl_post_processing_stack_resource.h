#pragma once
#include "opengl_resource.h"
#include <utility>
#include <vector>

namespace opengl_rendering
{
    struct OpenGlDrawResources;

    /// <summary>
    /// Purpose: Stores ordered fullscreen post-processing passes for execution by the renderer.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns effect draw-resource copies by value; no ownership transfer on access.
    /// Thread Safety: Not thread-safe; use from the render thread.
    /// </remarks>
    class OpenGlPostProcessingStackResource final : public IOpenGlResource
    {
      public:
        struct Effect final
        {
            OpenGlDrawResources draw_resources = {};
            float blend = 1.0F;
        };

        void bind() override {}
        void unbind() override {}

        void add_effect(Effect effect)
        {
            _effects.push_back(std::move(effect));
        }

        const std::vector<Effect>& get_effects() const
        {
            return _effects;
        }

      private:
        std::vector<Effect> _effects = {};
    };
}
