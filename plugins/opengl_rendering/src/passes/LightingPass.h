#pragma once
#include "ShadowPass.h"
#include "opengl_resources/opengl_buffers.h"
#include "opengl_uploader.h"
#include "opengl_resources/opengl_shader.h"
#include "tbx/async/job_system.h"
#include <cstddef>
#include <memory>

namespace opengl_rendering
{
    /// @brief
    /// Purpose: Resolves deferred lighting from the geometry attachments into the final color
    /// target.
    /// @details
    /// Ownership: Owns the fullscreen draw state and shader program it creates lazily.
    /// Thread Safety: Not thread-safe; render-thread only.
    class LightingPass final
    {
      public:
        LightingPass(
            OpenGlUploader& resource_manager,
            tbx::JobSystem& job_system,
            OpenGlGBuffer& gbuffer,
            const ShadowPass& shadow_pass);
        LightingPass(const LightingPass&) = delete;
        LightingPass& operator=(const LightingPass&) = delete;
        ~LightingPass() noexcept;

      public:
        /// @brief
        /// Purpose: Executes the fullscreen deferred lighting resolve for one frame.
        /// @details
        /// Ownership: Does not take ownership of the supplied payload.
        /// Thread Safety: Not thread-safe; render-thread only.
        void draw(
            const tbx::Color& clear_color,
            const tbx::Size& render_size,
            const tbx::LightingRenderInfo& lighting);

      private:
        bool ensure_initialized();
        bool ensure_static_shader_bindings();
        void upload_tiled_light_data(
            const tbx::Color& clear_color,
            const tbx::Size& render_size,
            const tbx::LightingRenderInfo& lighting);

      private:
        OpenGlUploader& _resource_manager;
        tbx::JobSystem& _job_system;
        OpenGlGBuffer& _gbuffer;
        const ShadowPass& _shadow_pass;
        std::shared_ptr<OpenGlShaderProgram> _shader_program = nullptr;
        uint32 _fullscreen_vertex_array = 0U;
        uint32 _lighting_info_buffer = 0U;
        uint32 _directional_shadow_cascades_buffer = 0U;
        uint32 _point_lights_buffer = 0U;
        uint32 _spot_lights_buffer = 0U;
        uint32 _area_lights_buffer = 0U;
        uint32 _spot_shadow_maps_buffer = 0U;
        uint32 _area_shadow_maps_buffer = 0U;
        uint32 _tile_light_spans_buffer = 0U;
        uint32 _tile_point_light_indices_buffer = 0U;
        uint32 _tile_spot_light_indices_buffer = 0U;
        uint32 _tile_area_light_indices_buffer = 0U;
        std::size_t _point_lights_buffer_capacity = 0U;
        std::size_t _lighting_info_buffer_capacity = 0U;
        std::size_t _directional_shadow_cascades_buffer_capacity = 0U;
        std::size_t _spot_lights_buffer_capacity = 0U;
        std::size_t _area_lights_buffer_capacity = 0U;
        std::size_t _spot_shadow_maps_buffer_capacity = 0U;
        std::size_t _area_shadow_maps_buffer_capacity = 0U;
        std::size_t _tile_light_spans_buffer_capacity = 0U;
        std::size_t _tile_point_light_indices_buffer_capacity = 0U;
        std::size_t _tile_spot_light_indices_buffer_capacity = 0U;
        std::size_t _tile_area_light_indices_buffer_capacity = 0U;
        bool _has_uploaded_static_shader_bindings = false;
        bool _has_reported_frame_failure = false;
    };
}
