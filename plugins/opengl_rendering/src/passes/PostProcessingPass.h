#pragma once
#include "opengl_resources/opengl_buffers.h"
#include "opengl_uploader.h"
#include "opengl_resources/opengl_shader.h"
#include "tbx/graphics/render_pipeline.h"
#include "tbx/math/size.h"
#include <memory>
#include <optional>
#include <vector>

namespace opengl_rendering
{
    /// @brief
    /// Purpose: Executes scene post-processing effects as fullscreen passes.
    /// @details
    /// Ownership: Owns intermediate post-processing framebuffers/textures and reuses shared
    /// renderer resources for shader/material lookups.
    /// Thread Safety: Not thread-safe; render-thread only.
    class PostProcessingPass final
    {
      public:
        PostProcessingPass(
            OpenGlUploader& resource_manager,
            OpenGlGBuffer& gbuffer);
        PostProcessingPass(const PostProcessingPass&) = delete;
        PostProcessingPass& operator=(const PostProcessingPass&) = delete;
        ~PostProcessingPass() noexcept;

      public:
        /// @brief
        /// Purpose: Executes enabled post-processing effects and writes results back to final
        /// color.
        /// @details
        /// Ownership: Does not take ownership of the supplied payload.
        /// Thread Safety: Not thread-safe; render-thread only.
        void draw(const tbx::Size& render_size, const std::optional<tbx::PostProcessing>& post);

      private:
        void destroy_scratch_targets() noexcept;
        bool ensure_scratch_targets(const tbx::Size& size);

      private:
        OpenGlUploader& _resource_manager;
        OpenGlGBuffer& _gbuffer;
        tbx::Size _scratch_size = {0U, 0U};
        std::vector<GLuint> _scratch_textures = {};
        std::vector<GLuint> _scratch_framebuffers = {};
    };
}
