#pragma once
#include "OpenGlDrawCalls.h"
#include "opengl_resources/opengl_buffers.h"
#include "opengl_resources/opengl_resource_manager.h"

namespace opengl_rendering
{
    /// @brief
    /// Purpose: Draws transparent surfaces into the resolved final-color target after lighting.
    /// @details
    /// Ownership: References shared render resources and the g-buffer owned by the renderer.
    /// Thread Safety: Not thread-safe; render-thread only.
    class TransparentPass final
    {
      public:
        TransparentPass(
            const OpenGlResourceManager& resource_manager,
            OpenGlGBuffer& gbuffer);
        TransparentPass(const TransparentPass&) = delete;
        TransparentPass& operator=(const TransparentPass&) = delete;
        ~TransparentPass() noexcept;

      public:
        /// @brief
        /// Purpose: Draws the current frame's transparent surfaces with alpha blending.
        /// @details
        /// Ownership: Does not take ownership of the supplied payload.
        /// Thread Safety: Not thread-safe; render-thread only.
        void draw(
            const tbx::Mat4& view_projection,
            const std::vector<TransparentDrawCall>& draw_calls);

      private:
        const OpenGlResourceManager& _resource_manager;
        OpenGlGBuffer& _gbuffer;
    };
}
