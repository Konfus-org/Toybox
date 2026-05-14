#pragma once
#include "tbx/interfaces/window.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/ecs/entity_registry.h"
#include "tbx/systems/graphics/pipeline/commands/render_pass.h"
#include "tbx/tbx_api.h"
#include "tbx/types/components/camera.h"
#include "tbx/types/components/light.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/components/post_processing.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/material.h"
#include "tbx/types/matrices.h"
#include "tbx/types/sphere.h"
#include "tbx/types/uuid.h"
#include "tbx/types/viewport.h"
#include <functional>
#include <memory>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Identifies where renderable geometry comes from before backend resources are
    /// resolved.
    enum class RenderDataGeometrySource
    {
        DynamicMesh,
        StaticMesh
    };

    /// @brief
    /// Purpose: Describes one scene renderable captured from ECS for backend-neutral render
    /// preparation.
    struct TBX_API RenderDataRenderable
    {
        Uuid entity_uuid = {};
        RenderDataGeometrySource geometry_source = RenderDataGeometrySource::DynamicMesh;
        std::shared_ptr<Mesh> dynamic_mesh = {};
        Handle static_mesh = {};
        MaterialInstance material = {};
        Transform transform = {};
        Sphere world_bounds = {};
        bool is_visible = true;
    };

    /// @brief
    /// Purpose: Describes one point light submitted to a graphics backend.
    struct TBX_API RenderDataPointLight
    {
        Uuid entity_uuid = {};
        PointLight light = {};
        Transform transform = {};
        bool is_visible = true;
    };

    /// @brief
    /// Purpose: Describes one spot light submitted to a graphics backend.
    struct TBX_API RenderDataSpotLight
    {
        Uuid entity_uuid = {};
        SpotLight light = {};
        Transform transform = {};
        bool is_visible = true;
    };

    /// @brief
    /// Purpose: Describes one area light submitted to a graphics backend.
    struct TBX_API RenderDataAreaLight
    {
        Uuid entity_uuid = {};
        AreaLight light = {};
        Transform transform = {};
        bool is_visible = true;
    };

    /// @brief
    /// Purpose: Describes one directional light submitted to a graphics backend.
    struct TBX_API RenderDataDirectionalLight
    {
        Uuid entity_uuid = {};
        DirectionalLight light = {};
        Transform transform = {};
        bool is_visible = true;
    };

    /// @brief
    /// Purpose: Describes one rendered directional shadow cascade.
    struct TBX_API RenderDataDirectionalShadowCascade
    {
        Mat4 light_view_projection = Mat4(1.0F);
        float split_depth = 0.0F;
        float normal_bias = 0.0F;
        float depth_bias = 0.0F;
        float blend_distance = 0.0F;
        Uuid texture = {};
    };

    /// @brief
    /// Purpose: Describes one point-light dual-paraboloid shadow map.
    struct TBX_API RenderDataPointShadowMap
    {
        Uuid entity_uuid = {};
        Mat4 world_to_light = Mat4(1.0F);
        float range = 1.0F;
        float normal_bias = 0.0F;
        float depth_bias = 0.0F;
        uint32 layer_offset = 0U;
    };

    /// @brief
    /// Purpose: Describes one projected shadow map used by spot or area lights.
    struct TBX_API RenderDataProjectedShadowMap
    {
        Uuid entity_uuid = {};
        Mat4 light_view_projection = Mat4(1.0F);
        float z_near = 0.1F;
        float z_far = 1.0F;
        float normal_bias = 0.0F;
        float depth_bias = 0.0F;
        uint32 texture_layer = 0U;
    };

    /// @brief
    /// Purpose: Describes the sky submitted for rendering and the transform used to place it.
    struct TBX_API RenderDataSky
    {
        Sky sky = Sky {
            .material = MaterialInstance(
                Handle("Materials/Skybox.mat"),
                MaterialParameterBindings {
                    MaterialParameter("color", Color::BLACK),
                    MaterialParameter("emissive", Color::BLACK),
                    MaterialParameter("color_texture_blend", 0.0F),
                }),
        };
        Transform transform = {};
    };

    /// @brief
    /// Purpose: Stores all backend-neutral scene, frame, and prepared draw data for one render.
    struct TBX_API RenderData
    {
        Window output_window = {};
        Size requested_resolution = {};
        uint64 frame_index = 0U;
        uint32 shadow_map_resolution = 1024U;
        float shadow_render_distance = 90.0F;
        float shadow_softness = 1.0F;
        float local_light_max_distance = 64.0F;
        float shadow_caster_max_distance = 96.0F;

        Size render_resolution = {};
        Viewport viewport = {};
        Camera camera = {};
        Transform camera_transform = {};
        /// Entity that owns the active Camera (reserved for future view-relative effects).
        Uuid active_camera_entity_id = {};
        Mat4 view_projection = Mat4(1.0F);
        Vec3 camera_position = {};

        Uuid view_uniform_buffer = {};
        bool frame_started = false;
        bool view_started = false;

        std::vector<RenderDataRenderable> renderables = {};
        std::vector<RenderDataPointLight> point_lights = {};
        std::vector<RenderDataSpotLight> spot_lights = {};
        std::vector<RenderDataAreaLight> area_lights = {};
        std::vector<RenderDataDirectionalLight> directional_lights = {};

        RenderDataSky sky = {};
        PostProcessing post_processing = {};
        bool has_skybox = false;

        Uuid directional_shadow_light_entity = {};
        std::vector<RenderDataDirectionalShadowCascade> directional_shadow_cascades = {};
        std::vector<RenderDataPointShadowMap> point_shadow_maps = {};
        std::vector<RenderDataProjectedShadowMap> spot_shadow_maps = {};
        std::vector<RenderDataProjectedShadowMap> area_shadow_maps = {};
        std::vector<GraphicsRenderPass> directional_shadow_passes = {};
        std::vector<GraphicsRenderPass> point_shadow_passes = {};
        std::vector<GraphicsRenderPass> spot_shadow_passes = {};
        std::vector<GraphicsRenderPass> area_shadow_passes = {};
        Uuid directional_shadow_texture = {};
        Uuid point_shadow_texture = {};
        Uuid spot_shadow_texture = {};
        Uuid area_shadow_texture = {};

        std::vector<Uuid> scene_color_targets = {};
        Uuid scene_color_target = {};
        Uuid scene_world_position_target = {};
        Uuid scene_albedo_target = {};
        Uuid scene_normal_target = {};
        Uuid scene_emissive_target = {};
        Uuid scene_material_target = {};
        Uuid scene_depth_target = {};
        std::vector<GraphicsRenderPass> lighting_passes = {};
        std::vector<GraphicsRenderPass> post_process_passes = {};

        Uuid fullscreen_quad_vertex_buffer = {};
        Uuid fullscreen_quad_index_buffer = {};
        uint32 fullscreen_quad_index_count = 0U;

        std::vector<GraphicsIndexedDrawCommand> skybox_commands = {};
        std::vector<GraphicsIndexedDrawCommand> opaque_commands = {};
        std::vector<GraphicsIndexedDrawCommand> alpha_cutout_commands = {};
        std::vector<GraphicsIndexedDrawCommand> transparent_commands = {};
    };

    /// @brief
    /// Purpose: Builds backend-neutral render data from the ECS scene.
    class TBX_API RenderDataBuilder final
    {
      public:
        explicit RenderDataBuilder(std::weak_ptr<EntityRegistry> registry);
        ~RenderDataBuilder() noexcept = default;

      public:
        RenderDataBuilder(const RenderDataBuilder&) = delete;
        RenderDataBuilder& operator=(const RenderDataBuilder&) = delete;
        RenderDataBuilder(RenderDataBuilder&&) noexcept = default;
        RenderDataBuilder& operator=(RenderDataBuilder&&) noexcept = default;

      public:
        /// @brief
        /// Purpose: Captures the current ECS render data into the render data.
        RenderData build(
            const Window& output_window,
            const Size& requested_resolution,
            uint64 frame_index,
            uint32 shadow_map_resolution,
            float shadow_render_distance,
            float shadow_softness,
            float local_light_max_distance,
            float shadow_caster_max_distance) const;

      private:
        void append_dynamic_meshes(RenderData& render_data) const;
        void append_lights(RenderData& render_data) const;
        void append_post_processing(RenderData& render_data) const;
        void append_sky(RenderData& render_data) const;
        void append_static_meshes(RenderData& render_data) const;
        MaterialInstance get_material(Entity& entity) const;

      private:
        std::weak_ptr<EntityRegistry> _registry;
        MaterialInstance _default_material = MaterialInstance(Handle("Materials/Flat.mat"));
    };
}
