#pragma once
#include "OpenGlFrameContext.h"
#include "opengl_resources/opengl_resource_manager.h"
#include "opengl_resources/opengl_shader.h"
#include <any>
#include <memory>

namespace opengl_rendering
{
    /// @brief
    /// Purpose: Renders realtime shadow maps for deferred directional, point, spot, and area
    /// lights.
    /// @details
    /// Ownership: Owns the shadow framebuffer, depth textures, and shadow shader program.
    /// Thread Safety: Not thread-safe; render-thread only.
    class ShadowPassOperation final
    {
      public:
        ShadowPassOperation(OpenGlResourceManager& resource_manager);
        ShadowPassOperation(const ShadowPassOperation&) = delete;
        ShadowPassOperation& operator=(const ShadowPassOperation&) = delete;
        ~ShadowPassOperation() noexcept;

        /// @brief
        /// Purpose: Renders all shadow-enabled lights into the pass-owned depth textures.
        /// @details
        /// Ownership: Does not take ownership of the supplied payload.
        /// Thread Safety: Not thread-safe; render-thread only.
        void execute(const std::any& payload);

        /// @brief
        /// Purpose: Returns the depth texture array used for directional cascades.
        /// @details
        /// Ownership: Returns a non-owning OpenGL texture handle managed by this pass.
        /// Thread Safety: Not thread-safe; render-thread only.
        tbx::uint32 get_directional_shadow_texture() const;

        /// @brief
        /// Purpose: Returns the cube-map array used for point-light shadows.
        /// @details
        /// Ownership: Returns a non-owning OpenGL texture handle managed by this pass.
        /// Thread Safety: Not thread-safe; render-thread only.
        tbx::uint32 get_point_shadow_texture() const;

        /// @brief
        /// Purpose: Returns the layered depth texture used for spot-light shadows.
        /// @details
        /// Ownership: Returns a non-owning OpenGL texture handle managed by this pass.
        /// Thread Safety: Not thread-safe; render-thread only.
        tbx::uint32 get_spot_shadow_texture() const;

        /// @brief
        /// Purpose: Returns the layered depth texture used for area-light shadows.
        /// @details
        /// Ownership: Returns a non-owning OpenGL texture handle managed by this pass.
        /// Thread Safety: Not thread-safe; render-thread only.
        tbx::uint32 get_area_shadow_texture() const;

      private:
        bool ensure_initialized();

      private:
        OpenGlResourceManager& _resource_manager;
        std::shared_ptr<OpenGlShaderProgram> _shader_program = nullptr;
        tbx::uint32 _framebuffer = 0U;
        tbx::uint32 _directional_shadow_texture = 0U;
        tbx::uint32 _point_shadow_texture = 0U;
        tbx::uint32 _spot_shadow_texture = 0U;
        tbx::uint32 _area_shadow_texture = 0U;
        tbx::uint32 _directional_shadow_resolution = 0U;
        tbx::uint32 _point_shadow_resolution = 0U;
        tbx::uint32 _spot_shadow_resolution = 0U;
        tbx::uint32 _area_shadow_resolution = 0U;
        tbx::uint32 _directional_shadow_layer_capacity = 0U;
        tbx::uint32 _point_shadow_light_capacity = 0U;
        tbx::uint32 _spot_shadow_layer_capacity = 0U;
        tbx::uint32 _area_shadow_layer_capacity = 0U;
        bool _has_reported_initialization_failure = false;
    };
}
