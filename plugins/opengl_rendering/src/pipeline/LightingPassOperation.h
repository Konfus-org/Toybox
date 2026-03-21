#pragma once
#include "OpenGlFrameContext.h"
#include "opengl_resources/opengl_buffers.h"
#include "opengl_resources/opengl_resource_manager.h"
#include "opengl_resources/opengl_shader.h"
#include "tbx/async/job_system.h"
#include <any>
#include <cstddef>
#include <memory>

namespace opengl_rendering
{
    /// <summary>
    /// Purpose: Resolves deferred lighting from the g-buffer into the final color target.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns the fullscreen draw state and shader program it creates lazily.
    /// Thread Safety: Not thread-safe; render-thread only.
    /// </remarks>
    class LightingPassOperation final
    {
      public:
        LightingPassOperation(
            OpenGlResourceManager& resource_manager,
            tbx::JobSystem& job_system,
            OpenGlGBuffer& gbuffer);
        LightingPassOperation(const LightingPassOperation&) = delete;
        LightingPassOperation& operator=(const LightingPassOperation&) = delete;
        ~LightingPassOperation() noexcept;

        /// <summary>
        /// Purpose: Executes the fullscreen deferred lighting resolve for one frame.
        /// Ownership: Does not take ownership of the supplied payload.
        /// Thread Safety: Not thread-safe; render-thread only.
        /// </summary>
        void execute(const std::any& payload);

      private:
        bool ensure_initialized();
        void upload_tiled_light_data(const OpenGlFrameContext& frame_context);

      private:
        OpenGlResourceManager& _resource_manager;
        tbx::JobSystem& _job_system;
        OpenGlGBuffer& _gbuffer;
        std::shared_ptr<OpenGlShaderProgram> _shader_program = nullptr;
        tbx::uint32 _fullscreen_vertex_array = 0U;
        tbx::uint32 _lighting_info_buffer = 0U;
        tbx::uint32 _point_lights_buffer = 0U;
        tbx::uint32 _spot_lights_buffer = 0U;
        tbx::uint32 _area_lights_buffer = 0U;
        tbx::uint32 _tile_light_spans_buffer = 0U;
        tbx::uint32 _tile_point_light_indices_buffer = 0U;
        tbx::uint32 _tile_spot_light_indices_buffer = 0U;
        tbx::uint32 _tile_area_light_indices_buffer = 0U;
        std::size_t _point_lights_buffer_capacity = 0U;
        std::size_t _lighting_info_buffer_capacity = 0U;
        std::size_t _spot_lights_buffer_capacity = 0U;
        std::size_t _area_lights_buffer_capacity = 0U;
        std::size_t _tile_light_spans_buffer_capacity = 0U;
        std::size_t _tile_point_light_indices_buffer_capacity = 0U;
        std::size_t _tile_spot_light_indices_buffer_capacity = 0U;
        std::size_t _tile_area_light_indices_buffer_capacity = 0U;
        bool _has_reported_frame_failure = false;
    };
}
