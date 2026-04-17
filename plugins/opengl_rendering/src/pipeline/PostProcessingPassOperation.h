#pragma once
#include "OpenGlFrameContext.h"
#include "opengl_resources/opengl_buffers.h"
#include "opengl_resources/opengl_resource_manager.h"
#include "opengl_resources/opengl_shader.h"
#include "tbx/math/size.h"
#include <any>
#include <memory>
#include <vector>

namespace opengl_rendering
{
    /// @brief
    /// Purpose: Executes scene post-processing effects as fullscreen passes.
    /// @details
    /// Ownership: Owns intermediate post-processing framebuffers/textures and reuses shared
    /// renderer resources for shader/material lookups.
    /// Thread Safety: Not thread-safe; render-thread only.
    class PostProcessingPassOperation final
    {
      public:
        PostProcessingPassOperation(
            OpenGlResourceManager& resource_manager,
            OpenGlGBuffer& gbuffer);
        PostProcessingPassOperation(const PostProcessingPassOperation&) = delete;
        PostProcessingPassOperation& operator=(const PostProcessingPassOperation&) = delete;
        ~PostProcessingPassOperation() noexcept;

        /// @brief
        /// Purpose: Executes enabled post-processing effects and writes results back to final
        /// color.
        /// @details
        /// Ownership: Does not take ownership of the supplied payload.
        /// Thread Safety: Not thread-safe; render-thread only.
        void execute(const std::any& payload);

      private:
        void destroy_scratch_targets() noexcept;
        bool ensure_scratch_targets(const tbx::Size& size);

      private:
        OpenGlResourceManager& _resource_manager;
        OpenGlGBuffer& _gbuffer;
        tbx::Size _scratch_size = {0U, 0U};
        std::vector<GLuint> _scratch_textures = {};
        std::vector<GLuint> _scratch_framebuffers = {};
    };
}
