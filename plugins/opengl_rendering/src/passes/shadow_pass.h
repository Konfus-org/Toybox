#pragma once
#include "open_gl_draw_calls.h"
#include "opengl_resources.h"
#include "opengl_resources/opengl_shader.h"
#include <memory>

namespace opengl_rendering
{
    /// @brief
    /// Purpose: Renders realtime shadow maps for deferred directional, point, spot, and area
    /// lights.
    /// @details
    /// Ownership: Owns the shadow framebuffer, depth textures, and shadow shader program.
    /// Thread Safety: Not thread-safe; render-thread only.
    class ShadowPass final
    {
      public:
        ShadowPass(OpenGlResources& resources);
        ShadowPass(const ShadowPass&) = delete;
        ShadowPass& operator=(const ShadowPass&) = delete;
        ~ShadowPass() noexcept;

      public:
        /// @brief
        /// Purpose: Renders all shadow-enabled lights into the pass-owned depth textures.
        /// @details
        /// Ownership: Does not take ownership of the supplied payload.
        /// Thread Safety: Not thread-safe; render-thread only.
        void draw(
            const tbx::ShadowRenderInfo& shadow_info,
            const std::vector<ShadowDrawCall>& draw_calls);

        /// @brief
        /// Purpose: Returns the depth texture array used for directional cascades.
        /// @details
        /// Ownership: Returns a non-owning OpenGL texture handle managed by this pass.
        /// Thread Safety: Not thread-safe; render-thread only.
        uint32 get_directional_shadow_texture() const;

        /// @brief
        /// Purpose: Returns the cube-map array used for point-light shadows.
        /// @details
        /// Ownership: Returns a non-owning OpenGL texture handle managed by this pass.
        /// Thread Safety: Not thread-safe; render-thread only.
        uint32 get_point_shadow_texture() const;

        /// @brief
        /// Purpose: Returns the layered depth texture used for spot-light shadows.
        /// @details
        /// Ownership: Returns a non-owning OpenGL texture handle managed by this pass.
        /// Thread Safety: Not thread-safe; render-thread only.
        uint32 get_spot_shadow_texture() const;

        /// @brief
        /// Purpose: Returns the layered depth texture used for area-light shadows.
        /// @details
        /// Ownership: Returns a non-owning OpenGL texture handle managed by this pass.
        /// Thread Safety: Not thread-safe; render-thread only.
        uint32 get_area_shadow_texture() const;

      private:
        bool ensure_initialized();

      private:
        OpenGlResources& _resources;
        std::shared_ptr<OpenGlShaderProgram> _shader_program = nullptr;
        uint32 _framebuffer = 0U;
        uint32 _directional_shadow_texture = 0U;
        uint32 _point_shadow_texture = 0U;
        uint32 _spot_shadow_texture = 0U;
        uint32 _area_shadow_texture = 0U;
        uint32 _directional_shadow_resolution = 0U;
        uint32 _point_shadow_resolution = 0U;
        uint32 _spot_shadow_resolution = 0U;
        uint32 _area_shadow_resolution = 0U;
        uint32 _directional_shadow_layer_capacity = 0U;
        uint32 _point_shadow_light_capacity = 0U;
        uint32 _spot_shadow_layer_capacity = 0U;
        uint32 _area_shadow_layer_capacity = 0U;
        uint32 _directional_shadow_internal_format = 0U;
        uint32 _point_shadow_internal_format = 0U;
        uint32 _spot_shadow_internal_format = 0U;
        uint32 _area_shadow_internal_format = 0U;
        bool _has_reported_initialization_failure = false;
        bool _has_reported_depth_format_fallback = false;
        bool _has_reported_resolution_fallback = false;
        bool _has_reported_depth_format_failure = false;
    };
}
