#pragma once
#include "opengl_context.h"
#include "opengl_resources/opengl_buffers.h"
#include "opengl_resources/opengl_resource_manager.h"
#include "pipeline/opengl_render_pipeline.h"
#include "tbx/assets/asset_manager.h"
#include "tbx/common/int.h"
#include "tbx/common/uuid.h"
#include "tbx/ecs/entity_registry.h"
#include "tbx/graphics/messages.h"
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace tbx::plugins
{
    /// <summary>Configures directional shadow behavior for the OpenGL renderer.</summary>
    /// <remarks>Purpose: Groups runtime-tunable shadow map and light-space camera settings.
    /// Ownership: Value type owned by caller/renderer by copy.
    /// Thread Safety: Safe to copy; mutate and consume on render thread.</remarks>
    struct OpenGlShadowSettings final
    {
        uint32 shadow_map_resolution = 2048U;
        float shadow_render_distance = 90.0F;
        float shadow_softness = 1.75F;
    };

    /// <summary>Owns one window-bound OpenGL renderer state and frame execution.</summary>
    /// <remarks>Purpose: Manages GL initialization, scene extraction, and pipeline execution.
    /// Ownership: Owns render pipeline and all renderer-scoped GPU resource wrappers.
    /// Thread Safety: Not thread-safe; use on render thread only.</remarks>
    struct OpenGlRenderer final
    {
        OpenGlRenderer(
            GraphicsProcAddress loader,
            EntityRegistry& entity_registry,
            AssetManager& asset_manager,
            OpenGlContext context,
            const OpenGlShadowSettings& shadow_settings);
        ~OpenGlRenderer() noexcept;

        /// <summary>Renders and presents one frame to the bound context window.</summary>
        /// <remarks>Purpose: Makes context current, builds frame data, runs pipeline,
        /// presents. Ownership: Uses renderer-owned resources and non-owning host references.
        /// Thread Safety: Call on render thread only.</remarks>
        bool render();

        /// <summary>Updates current viewport size used for final presentation.</summary>
        /// <remarks>Purpose: Tracks current window output dimensions.
        /// Ownership: Stores value in renderer-owned state.
        /// Thread Safety: Call on render thread.</remarks>
        void set_viewport_size(const Size& viewport_size);

        /// <summary>Schedules a new render resolution to apply before next frame.</summary>
        /// <remarks>Purpose: Defers expensive framebuffer resize to render path.
        /// Ownership: Stores optional value in renderer-owned state.
        /// Thread Safety: Call on render thread.</remarks>
        void set_pending_render_resolution(const std::optional<Size>& pending_render_resolution);

        /// <summary>Updates directional shadow tuning values used for subsequent frames.</summary>
        /// <remarks>Purpose: Applies runtime graphics settings to shadow resource allocation and
        /// matrix generation. Ownership: Stores a value copy in renderer-owned state.
        /// Thread Safety: Call on render thread.</remarks>
        void set_shadow_settings(const OpenGlShadowSettings& shadow_settings);

        /// <summary>Enables or disables compute light-culling execution.</summary>
        /// <remarks>Purpose: Toggles compute-tiled light indexing.
        /// Ownership: Stores the toggle value by copy.
        /// Thread Safety: Call on render thread.</remarks>
        void set_compute_culling_enabled(bool is_enabled);

        /// <summary>Enables or disables instanced local-light volume rendering.</summary>
        /// <remarks>Purpose: Toggles the local-light volume accumulation pass.
        /// Ownership: Stores the toggle value by copy.
        /// Thread Safety: Call on render thread.</remarks>
        void set_local_light_volume_enabled(bool is_enabled);

        /// <summary>Enables or disables GPU pass timing collection/logging.</summary>
        /// <remarks>Purpose: Controls render-pipeline GPU timing instrumentation.
        /// Ownership: Stores the toggle value by copy.
        /// Thread Safety: Call on render thread.</remarks>
        void set_gpu_pass_timing_enabled(bool is_enabled);

      private:
        void initialize();
        void shutdown();
        void reset_shadow_maps();
        void set_render_resolution(const Size& render_resolution);

      private:
        std::reference_wrapper<EntityRegistry> _entity_registry;
        OpenGlContext _context;

        std::unique_ptr<OpenGlResourceManager> _resource_manager = nullptr;
        std::unique_ptr<OpenGlRenderPipeline> _render_pipeline = nullptr;

        Size _viewport_size = {0, 0};
        Size _render_resolution = {0, 0};
        std::optional<Size> _pending_render_resolution = std::nullopt;

        Uuid _gbuffer_resource = Uuid::NONE;
        Uuid _lighting_framebuffer_resource = Uuid::NONE;
        Uuid _post_process_ping_framebuffer_resource = Uuid::NONE;
        Uuid _post_process_pong_framebuffer_resource = Uuid::NONE;
        Uuid _pinned_sky_resource = Uuid::NONE;
        Uuid _deferred_lighting_resource = Uuid::NONE;
        std::unique_ptr<OpenGlStorageBuffer> _packed_lights_buffer = nullptr;
        std::unique_ptr<OpenGlStorageBuffer> _tile_headers_buffer = nullptr;
        std::unique_ptr<OpenGlStorageBuffer> _tile_light_indices_buffer = nullptr;
        std::unique_ptr<OpenGlStorageBuffer> _tile_overflow_counter_buffer = nullptr;
        std::unique_ptr<OpenGlStorageBuffer> _local_light_indices_buffer = nullptr;
        std::shared_ptr<OpenGlShaderProgram> _light_culling_shader_program = nullptr;
        std::shared_ptr<OpenGlShaderProgram> _local_light_volume_shader_program = nullptr;
        std::vector<Uuid> _shadow_map_resources = {};
        OpenGlShadowSettings _shadow_settings = {};
        uint32 _light_tile_size = 16U;
        uint32 _max_lights_per_tile = 256U;
        bool _is_compute_culling_enabled = true;
        bool _is_local_light_volume_enabled = true;
        bool _is_gpu_pass_timing_enabled = false;
    };
}
