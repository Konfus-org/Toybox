#pragma once
#include "OpenGlFrameContext.h"
#include "opengl_resources/opengl_resource_manager.h"
#include "opengl_resources/opengl_shader.h"
#include <any>
#include <memory>

namespace opengl_rendering
{
    /// <summary>
    /// Purpose: Renders stabilized cascaded directional shadow maps for the lighting pass.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns the shadow framebuffer, texture array, and shader program it creates.
    /// Thread Safety: Not thread-safe; render-thread only.
    /// </remarks>
    class ShadowPassOperation final
    {
      public:
        ShadowPassOperation(OpenGlResourceManager& resource_manager);
        ShadowPassOperation(const ShadowPassOperation&) = delete;
        ShadowPassOperation& operator=(const ShadowPassOperation&) = delete;
        ~ShadowPassOperation() noexcept;

        /// <summary>
        /// Purpose: Renders all configured directional shadow cascades for one frame.
        /// Ownership: Does not take ownership of the supplied payload.
        /// Thread Safety: Not thread-safe; render-thread only.
        /// </summary>
        void execute(const std::any& payload);

        /// <summary>
        /// Purpose: Returns the OpenGL texture-array object containing the shadow cascades.
        /// Ownership: The operation retains ownership of the returned handle.
        /// Thread Safety: Not thread-safe; render-thread only.
        /// </summary>
        tbx::uint32 get_shadow_texture() const;

      private:
        bool ensure_initialized();
        bool ensure_shadow_targets(const OpenGlFrameContext& frame_context);

      private:
        OpenGlResourceManager& _resource_manager;
        std::shared_ptr<OpenGlShaderProgram> _shader_program = nullptr;
        tbx::uint32 _framebuffer = 0U;
        tbx::uint32 _shadow_texture = 0U;
        tbx::uint32 _shadow_sampler = 0U;
        tbx::uint32 _resolution = 0U;
    };
}
