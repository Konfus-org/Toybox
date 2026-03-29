#pragma once
#include "OpenGlFrameContext.h"
#include "opengl_resources/opengl_buffers.h"
#include "opengl_resources/opengl_resource_manager.h"
#include <any>

namespace opengl_rendering
{
    /// @brief
    /// Purpose: Draws transparent surfaces into the resolved final-color target after lighting.

    /// @details
    /// Ownership: References shared render resources and the g-buffer owned by the renderer.
    /// Thread Safety: Not thread-safe; render-thread only.

    class TransparentPassOperation final
    {
      public:
        TransparentPassOperation(
            const OpenGlResourceManager& resource_manager,
            OpenGlGBuffer& gbuffer);
        TransparentPassOperation(const TransparentPassOperation&) = delete;
        TransparentPassOperation& operator=(const TransparentPassOperation&) = delete;
        ~TransparentPassOperation() noexcept;

        /// @brief
        /// Purpose: Draws the current frame's transparent surfaces with alpha blending.
        /// @details
        /// Ownership: Does not take ownership of the supplied payload.
        /// Thread Safety: Not thread-safe; render-thread only.

        void execute(const std::any& payload);

      private:
        const OpenGlResourceManager& _resource_manager;
        OpenGlGBuffer& _gbuffer;
    };
}
