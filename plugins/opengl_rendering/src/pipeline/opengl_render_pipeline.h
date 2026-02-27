#pragma once
#include "opengl_resources/opengl_buffers.h"
#include "opengl_resources/opengl_gbuffer.h"
#include "opengl_resources/opengl_resource_manager.h"
#include "tbx/common/handle.h"
#include "tbx/common/int.h"
#include "tbx/common/pipeline.h"
#include "tbx/common/uuid.h"
#include "tbx/ecs/entity.h"
#include "tbx/graphics/color.h"
#include "tbx/math/matrices.h"
#include "tbx/math/size.h"
#include "tbx/math/vectors.h"
#include <any>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace opengl_rendering
{
    /// <summary>
    /// Purpose: Captures the active camera entity used for frame visibility and matrix generation.
    /// </summary>
    /// <remarks>
    /// Ownership: Stores a non-owning entity wrapper whose underlying registry must outlive frame
    /// execution.
    /// Thread Safety: Read-only on the render thread.
    /// </remarks>
    struct OpenGlCameraView final
    {
        /// <summary>
        /// Purpose: Active camera entity used to derive view and projection state.
        /// Ownership: Non-owning entity wrapper; registry and entity must outlive frame execution.
        /// Thread Safety: Read-only on the render thread.
        /// </summary>
        tbx::Entity camera_entity = {};

        /// <summary>
        /// Purpose: Static-mesh entities visible to the active camera and ready for rendering.
        /// Ownership: Stores non-owning entity wrappers into the owning entity registry.
        /// Thread Safety: Read-only on the render thread.
        /// </summary>
        std::vector<tbx::Entity> in_view_static_entities = {};

        /// <summary>
        /// Purpose: Dynamic-mesh entities visible to the active camera and ready for rendering.
        /// Ownership: Stores non-owning entity wrappers into the owning entity registry.
        /// Thread Safety: Read-only on the render thread.
        /// </summary>
        std::vector<tbx::Entity> in_view_dynamic_entities = {};
    };

    /// <summary>
    /// Purpose: Describes post-processing configuration for the current frame.
    /// </summary>
    /// <remarks>
    /// Ownership: Stores value settings and a non-owning material handle reference.
    /// Thread Safety: Safe to read concurrently after construction.
    /// </remarks>
    struct OpenGlPostProcessEffect final
    {
        /// <summary>
        /// Purpose: Source entity that owns the PostProcessing component.
        /// Ownership: Non-owning entity wrapper.
        /// Thread Safety: Safe to read on render thread while registry is valid.
        /// </summary>
        tbx::Entity owner_entity = {};

        /// <summary>
        /// Purpose: Effect index within the owner's PostProcessing component.
        /// Ownership: Value type.
        /// Thread Safety: Safe to read concurrently.
        /// </summary>
        size_t source_effect_index = 0U;

        /// <summary>
        /// Purpose: Enables or disables this post-process effect.
        /// Ownership: Value type.
        /// Thread Safety: Safe to read concurrently.
        /// </summary>
        bool is_enabled = false;

        /// <summary>
        /// Purpose: tbx::Runtime material data used to shade this fullscreen post-processing pass.
        /// Ownership: Owns parameter/texture values and a base material handle reference.
        /// Thread Safety: Safe to read concurrently.
        /// </summary>
        MaterialInstance material = {};

        /// <summary>
        /// Purpose: Controls blend weight between source scene color and post-processed output.
        /// Ownership: Value type.
        /// Thread Safety: Safe to read concurrently.
        /// </summary>
        float blend = 1.0f;
    };

    /// <summary>
    /// Purpose: Describes post-processing configuration for the current frame.
    /// </summary>
    /// <remarks>
    /// Ownership: Stores value settings and a non-owning view over effect data owned by caller.
    /// Thread Safety: Safe to read concurrently after construction.
    /// </remarks>
    struct OpenGlPostProcessSettings final
    {
        /// <summary>
        /// Purpose: Enables or disables execution of post-processing.
        /// Ownership: Value type.
        /// Thread Safety: Safe to read concurrently.
        /// </summary>
        bool is_enabled = false;

        /// <summary>
        /// Purpose: tbx::Entity that owns the active PostProcessing component.
        /// Ownership: Non-owning entity wrapper into the active registry.
        /// Thread Safety: Read-only on render thread.
        /// </summary>
        tbx::Entity owner_entity = {};

        /// <summary>
        /// Purpose: Ordered effects to execute from first to last.
        /// Ownership: Non-owning span; caller owns underlying storage.
        /// Thread Safety: Safe to read concurrently while underlying storage remains valid.
        /// </summary>
        std::span<const OpenGlPostProcessEffect> effects = {};
    };

    /// <summary>
    /// Purpose: Packs one directional light for deferred shading.
    /// </summary>
    /// <remarks>
    /// Ownership: Value type.
    /// Thread Safety: Safe to read concurrently after construction.
    /// </remarks>
    struct OpenGlDirectionalLightData final
    {
        tbx::Vec3 direction = tbx::Vec3(0.0f, -1.0f, 0.0f);
        float intensity = 1.0f;
        tbx::Vec3 color = tbx::Vec3(1.0f, 1.0f, 1.0f);
        float ambient = 0.03f;
        int shadow_map_index = -1;
    };

    /// <summary>
    /// Purpose: Packs one point light for deferred shading.
    /// </summary>
    /// <remarks>
    /// Ownership: Value type.
    /// Thread Safety: Safe to read concurrently after construction.
    /// </remarks>
    struct OpenGlPointLightData final
    {
        tbx::Vec3 position = tbx::Vec3(0.0f);
        float range = 10.0f;
        tbx::Vec3 color = tbx::Vec3(1.0f, 1.0f, 1.0f);
        float intensity = 1.0f;
        int shadow_map_index = -1;
    };

    /// <summary>
    /// Purpose: Packs one spot light for deferred shading.
    /// </summary>
    /// <remarks>
    /// Ownership: Value type.
    /// Thread Safety: Safe to read concurrently after construction.
    /// </remarks>
    struct OpenGlSpotLightData final
    {
        tbx::Vec3 position = tbx::Vec3(0.0f);
        float range = 10.0f;
        tbx::Vec3 direction = tbx::Vec3(0.0f, -1.0f, 0.0f);
        float inner_cos = 0.94f;
        tbx::Vec3 color = tbx::Vec3(1.0f, 1.0f, 1.0f);
        float outer_cos = 0.82f;
        float intensity = 1.0f;
        int shadow_map_index = -1;
    };

    /// <summary>
    /// Purpose: Packs one area light for deferred shading.
    /// </summary>
    /// <remarks>
    /// Ownership: Value type.
    /// Thread Safety: Safe to read concurrently after construction.
    /// </remarks>
    struct OpenGlAreaLightData final
    {
        tbx::Vec3 position = tbx::Vec3(0.0f);
        float range = 10.0f;
        tbx::Vec3 direction = tbx::Vec3(0.0f, -1.0f, 0.0f);
        float intensity = 1.0f;
        tbx::Vec3 color = tbx::Vec3(1.0f, 1.0f, 1.0f);
        tbx::Vec2 area_size = tbx::Vec2(1.0f, 1.0f);
    };

    /// <summary>
    /// Purpose: Identifies supported packed deferred-light categories.
    /// </summary>
    /// <remarks>
    /// Ownership: Value enum.
    /// Thread Safety: Safe to read concurrently.
    /// </remarks>
    enum class OpenGlPackedLightType : int
    {
        Directional = 0,
        Point = 1,
        Spot = 2,
        Area = 3,
    };

    /// <summary>
    /// Purpose: Packs one light into std430-friendly vec4/ivec4 fields for SSBO uploads.
    /// </summary>
    /// <remarks>
    /// Ownership: Value type.
    /// Thread Safety: Safe to read concurrently after construction.
    /// </remarks>
    struct OpenGlPackedLightData final
    {
        tbx::Vec4 position_range = tbx::Vec4(0.0f, 0.0f, 0.0f, 0.0f);
        tbx::Vec4 direction_inner_cos = tbx::Vec4(0.0f, -1.0f, 0.0f, 0.0f);
        tbx::Vec4 color_intensity = tbx::Vec4(1.0f, 1.0f, 1.0f, 1.0f);
        tbx::Vec4 area_outer_ambient = tbx::Vec4(1.0f, 1.0f, 0.0f, 0.03f);
        IVec4 metadata = IVec4(static_cast<int>(OpenGlPackedLightType::Directional), -1, 0, 0);
    };

    /// <summary>
    /// Purpose: Describes compute light-culling SSBO bindings and tile dimensions.
    /// </summary>
    /// <remarks>
    /// Ownership: Value type; stores OpenGL object identifiers by value.
    /// Thread Safety: Safe to read concurrently after construction.
    /// </remarks>
    struct OpenGlLightCullingFrameData final
    {
        uint32 tile_size = 16U;
        uint32 tile_count_x = 0U;
        uint32 tile_count_y = 0U;
        uint32 max_lights_per_tile = 256U;
        uint32 packed_light_count = 0U;
        uint32 packed_lights_buffer_id = 0U;
        uint32 tile_headers_buffer_id = 0U;
        uint32 tile_light_indices_buffer_id = 0U;
        uint32 tile_overflow_counter_buffer_id = 0U;
    };

    /// <summary>
    /// Purpose: Describes instanced local-light draw inputs.
    /// </summary>
    /// <remarks>
    /// Ownership: Value type; stores OpenGL object identifiers by value.
    /// Thread Safety: Safe to read concurrently after construction.
    /// </remarks>
    struct OpenGlLocalLightVolumeFrameData final
    {
        uint32 local_light_indices_buffer_id = 0U;
        uint32 point_light_count = 0U;
        uint32 spot_light_count = 0U;
        uint32 area_light_count = 0U;
    };

    /// <summary>
    /// Purpose: Describes shadow-map resources used by deferred shading.
    /// </summary>
    /// <remarks>
    /// Ownership: Stores non-owning spans over caller-managed arrays.
    /// Thread Safety: Safe to read concurrently while storage remains valid.
    /// </remarks>
    struct OpenGlShadowFrameData final
    {
        /// <summary>
        /// Purpose: Ordered shadow-map resources sampled by deferred lighting.
        /// Ownership: Non-owning span; caller owns shadow-map UUID storage.
        /// Thread Safety: Safe to read concurrently while storage remains valid.
        /// </summary>
        std::span<const Uuid> map_uuids = {};

        /// <summary>
        /// Purpose: Ordered light view-projection matrices matching each shadow map.
        /// Ownership: Non-owning span; caller owns matrix storage.
        /// Thread Safety: Safe to read concurrently while storage remains valid.
        /// </summary>
        std::span<const tbx::Mat4> light_view_projections = {};

        /// <summary>
        /// Purpose: Cascade split distances used for cascaded directional shadow sampling.
        /// Ownership: Non-owning span; caller owns split storage.
        /// Thread Safety: Safe to read concurrently while storage remains valid.
        /// </summary>
        std::span<const float> cascade_splits = {};

        /// <summary>
        /// Purpose: Resolution shared by all shadow-map textures rendered this frame.
        /// Ownership: Value type.
        /// Thread Safety: Safe to read concurrently.
        /// </summary>
        uint32 shadow_map_resolution = 1U;

        /// <summary>
        /// Purpose: Directional shadow filter radius measured in shadow-map texels.
        /// Ownership: Value type.
        /// Thread Safety: Safe to read concurrently.
        /// </summary>
        float shadow_softness = 1.75F;

        /// <summary>
        /// Purpose: Maximum distance from camera to render shadows.
        /// Ownership: Value type.
        /// Thread Safety: Safe to read concurrently.
        /// </summary>
        float shadow_render_distance = 90.0F;
    };

    /// <summary>
    /// Purpose: Provides immutable per-frame data to OpenGL render operations.
    /// </summary>
    /// <remarks>
    /// Ownership: Stores non-owning pointers and wrappers to scene resources; owners must outlive
    /// frame execution.
    /// Thread Safety: Safe to read concurrently after construction.
    /// </remarks>
    struct OpenGlRenderFrameContext
    {
        /// <summary>
        /// Purpose: Active camera and visible-entity list used by geometry rendering.
        /// Ownership: Value type owned by this context; internally uses non-owning entity
        /// wrappers.
        /// Thread Safety: Read-only on the render thread.
        /// </summary>
        OpenGlCameraView camera_view = {};

        /// <summary>
        /// Purpose: Internal render resolution used for scene rasterization.
        /// Ownership: Value type owned by this context.
        /// Thread Safety: Safe to read concurrently.
        /// </summary>
        Size render_resolution = {};

        /// <summary>
        /// Purpose: Output viewport size used for final presentation.
        /// Ownership: Value type owned by this context.
        /// Thread Safety: Safe to read concurrently.
        /// </summary>
        Size viewport_size = {};

        /// <summary>
        /// Purpose: Clear color used when starting the geometry pass.
        /// Ownership: Value type owned by this context.
        /// Thread Safety: Safe to read concurrently.
        /// </summary>
        tbx::Color clear_color = Color::BLACK;

        /// <summary>
        /// Purpose: Optional sky entity used by the sky pass.
        /// Ownership: Non-owning entity wrapper into the active registry.
        /// Thread Safety: Read-only on render thread.
        /// </summary>
        tbx::Entity sky_entity = {};

        /// <summary>
        /// Purpose: Optional post-processing settings used for final fullscreen shading.
        /// Ownership: Value type owned by this context.
        /// Thread Safety: Safe to read concurrently.
        /// </summary>
        OpenGlPostProcessSettings post_process = {};

        /// <summary>
        /// Purpose: tbx::Entity used to resolve deferred-lighting fullscreen material resources.
        /// Ownership: Non-owning entity wrapper into the active registry.
        /// Thread Safety: Read-only on render thread.
        /// </summary>
        tbx::Entity deferred_lighting_entity = {};

        /// <summary>
        /// Purpose: G-buffer used for deferred geometry outputs.
        /// Ownership: Non-owning pointer; caller retains G-buffer lifetime.
        /// Thread Safety: Use on the render thread.
        /// </summary>
        OpenGlGBuffer* gbuffer = nullptr;

        /// <summary>
        /// Purpose: Framebuffer used for deferred lighting composition before post-processing.
        /// Ownership: Non-owning pointer; caller retains framebuffer lifetime.
        /// Thread Safety: Use on the render thread.
        /// </summary>
        OpenGlFrameBuffer* lighting_target = nullptr;

        /// <summary>
        /// Purpose: First intermediate framebuffer used for multi-pass post-processing.
        /// Ownership: Non-owning pointer; caller retains framebuffer lifetime.
        /// Thread Safety: Use on the render thread.
        /// </summary>
        OpenGlFrameBuffer* post_process_ping_target = nullptr;

        /// <summary>
        /// Purpose: Second intermediate framebuffer used for multi-pass post-processing.
        /// Ownership: Non-owning pointer; caller retains framebuffer lifetime.
        /// Thread Safety: Use on the render thread.
        /// </summary>
        OpenGlFrameBuffer* post_process_pong_target = nullptr;

        /// <summary>
        /// Purpose: Camera world position used for deferred specular lighting.
        /// Ownership: Value type owned by this context.
        /// Thread Safety: Safe to read concurrently.
        /// </summary>
        tbx::Vec3 camera_world_position = tbx::Vec3(0.0f);

        /// <summary>
        /// Purpose: Camera forward direction used for directional shadow cascade selection.
        /// Ownership: Value type owned by this context.
        /// Thread Safety: Safe to read concurrently.
        /// </summary>
        tbx::Vec3 camera_forward = tbx::Vec3(0.0f, 0.0f, -1.0f);

        /// <summary>
        /// Purpose: Camera view-projection matrix for the active frame.
        /// Ownership: Value type owned by this context.
        /// Thread Safety: Safe to read concurrently.
        /// </summary>
        tbx::Mat4 view_projection = tbx::Mat4(1.0f);

        /// <summary>
        /// Purpose: Inverse of the camera view-projection matrix.
        /// Ownership: Value type owned by this context.
        /// Thread Safety: Safe to read concurrently.
        /// </summary>
        tbx::Mat4 inverse_view_projection = tbx::Mat4(1.0f);

        /// <summary>
        /// Purpose: Packed lights visible to deferred and local-volume lighting passes.
        /// Ownership: Non-owning span; caller owns underlying storage.
        /// Thread Safety: Safe to read concurrently while storage remains valid.
        /// </summary>
        std::span<const OpenGlPackedLightData> packed_lights = {};

        /// <summary>
        /// Purpose: Per-frame compute light-culling bindings and dispatch dimensions.
        /// Ownership: Value type owned by this context.
        /// Thread Safety: Safe to read concurrently.
        /// </summary>
        OpenGlLightCullingFrameData light_culling = {};

        /// <summary>
        /// Purpose: Per-frame local-light instancing input bindings and counts.
        /// Ownership: Value type owned by this context.
        /// Thread Safety: Safe to read concurrently.
        /// </summary>
        OpenGlLocalLightVolumeFrameData local_light_volumes = {};

        /// <summary>
        /// Purpose: Enables compute-tiled local-light indexing for deferred lighting.
        /// Ownership: Value type owned by this context.
        /// Thread Safety: Safe to read concurrently.
        /// </summary>
        bool is_compute_culling_enabled = true;

        /// <summary>
        /// Purpose: Enables instanced local-light volume accumulation.
        /// Ownership: Value type owned by this context.
        /// Thread Safety: Safe to read concurrently.
        /// </summary>
        bool is_local_light_volume_enabled = true;

        /// <summary>
        /// Purpose: Enables GPU pass timing instrumentation and logging.
        /// Ownership: Value type owned by this context.
        /// Thread Safety: Safe to read concurrently.
        /// </summary>
        bool is_gpu_pass_timing_enabled = false;

        /// <summary>
        /// Purpose: tbx::Shader program used to dispatch compute light culling.
        /// Ownership: Non-owning pointer; caller retains shader-program lifetime.
        /// Thread Safety: Use on render thread.
        /// </summary>
        OpenGlShaderProgram* light_culling_shader_program = nullptr;

        /// <summary>
        /// Purpose: tbx::Shader program used by instanced local-light volume rendering.
        /// Ownership: Non-owning pointer; caller retains shader-program lifetime.
        /// Thread Safety: Use on render thread.
        /// </summary>
        OpenGlShaderProgram* local_light_volume_shader_program = nullptr;

        /// <summary>
        /// Purpose: Shadow-map textures and transform inputs used by shadow and deferred passes.
        /// Ownership: Value type with non-owning spans; caller owns backing arrays.
        /// Thread Safety: Safe to read concurrently while storage remains valid.
        /// </summary>
        OpenGlShadowFrameData shadow_data = {};

        /// <summary>
        /// Purpose: Defines how the render target is scaled into the presentation target.
        /// Ownership: Value type owned by this context.
        /// Thread Safety: Safe to read concurrently.
        /// </summary>
        OpenGlFrameBufferPresentMode present_mode = OpenGlFrameBufferPresentMode::ASPECT_FIT;

        /// <summary>
        /// Purpose: Framebuffer identifier used as the destination draw target for presentation.
        /// Ownership: Value type owned by this context.
        /// Thread Safety: Safe to read concurrently.
        /// </summary>
        uint32 present_target_framebuffer_id = 0;

        /// <summary>
        /// Purpose: Scene color texture sampled by post-process passes.
        /// Ownership: Value type; texture is owned by the originating framebuffer resource.
        /// Thread Safety: Safe to read concurrently.
        /// </summary>
        uint32 scene_color_texture_id = 0;
    };

    /// <summary>
    /// Purpose: Represents a render-thread operation that consumes a per-frame OpenGL context.
    /// </summary>
    /// <remarks>
    /// Ownership: Owned by OpenGlRenderPipeline through unique pointers.
    /// Thread Safety: Not thread-safe; execute and mutate on the render thread.
    /// </remarks>
    class OpenGlRenderOperation : public PipelineOperation
    {
      public:
        /// <summary>
        /// Purpose: Executes this operation using a payload-backed frame context.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not take ownership of frame context resources.
        /// Thread Safety: Call only on the render thread.
        /// </remarks>
        void execute(const std::any& payload) override;

      protected:
        /// <summary>
        /// Purpose: Executes this operation using the provided frame context.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not take ownership of the frame context.
        /// Thread Safety: Call only on the render thread.
        /// </remarks>
        virtual void execute_with_frame_context(const OpenGlRenderFrameContext& frame_context) = 0;
    };

    /// <summary>
    /// Purpose: Executes OpenGL rendering work in a predefined operation order.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns render operations and stores a non-owning resource-manager reference.
    /// Thread Safety: Not thread-safe; configure and execute on the render thread.
    /// </remarks>
    class OpenGlRenderPipeline final : public Pipeline
    {
      public:
        /// <summary>
        /// Purpose: Configures the default OpenGL rendering operation sequence.
        /// </summary>
        /// <remarks>
        /// Ownership: Owns created operations; stores a non-owning pointer to the provided
        /// resource manager.
        /// Thread Safety: Construct on the render thread.
        /// </remarks>
        OpenGlRenderPipeline(OpenGlResourceManager& resource_manager);

        /// <summary>
        /// Purpose: Destroys the pipeline and clears queued operations.
        /// </summary>
        /// <remarks>
        /// Ownership: Releases owned operations.
        /// Thread Safety: Destroy on the render thread.
        /// </remarks>
        ~OpenGlRenderPipeline() noexcept override;

        /// <summary>
        /// Purpose: Executes the configured operation sequence using payload frame data.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not take ownership of payload resources.
        /// Thread Safety: Call only on the render thread.
        /// </remarks>
        void execute(const std::any& payload) override;

      private:
        OpenGlResourceManager* _resource_manager = nullptr;
    };
}
