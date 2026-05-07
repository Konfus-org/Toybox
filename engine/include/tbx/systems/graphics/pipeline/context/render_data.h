#pragma once
#include "tbx/interfaces/window_manager.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/ecs/entity_registry.h"
#include "tbx/systems/graphics/camera.h"
#include "tbx/systems/graphics/light.h"
#include "tbx/systems/graphics/material.h"
#include "tbx/systems/graphics/mesh.h"
#include "tbx/systems/graphics/pipeline/commands/render_pass.h"
#include "tbx/systems/graphics/post_processing.h"
#include "tbx/systems/graphics/resource_manager.h"
#include "tbx/systems/graphics/viewport.h"
#include "tbx/systems/math/matrices.h"
#include "tbx/systems/math/transform.h"
#include "tbx/tbx_api.h"
#include "tbx/types/uuid.h"
#include <functional>
#include <memory>
#include <vector>

namespace tbx
{
    class IGraphicsBackend;

    /// @brief
    /// Purpose: Stores frame-level inputs, derived camera state, and backend lifecycle flags.
    struct TBX_API FrameData
    {
        std::reference_wrapper<IGraphicsBackend> backend;
        std::reference_wrapper<GraphicsResourceManager> resource_manager;
        std::reference_wrapper<EntityRegistry> entity_registry;
        std::reference_wrapper<IWindowManager> window_manager;
        Window output_window = {};
        Size requested_resolution = {};
        uint64 frame_index = 0U;

        Size render_resolution = {};
        Viewport viewport = {};
        Camera camera = {};
        Transform camera_transform = {};
        Mat4 view_projection = Mat4(1.0F);
        Vec3 camera_position = {};

        Uuid view_uniform_buffer = {};
        bool frame_started = false;
        bool view_started = false;
    };

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
        RenderData(FrameData frame_data);

        FrameData frame;
        std::vector<RenderDataRenderable> renderables = {};
        std::vector<RenderDataPointLight> point_lights = {};
        std::vector<RenderDataSpotLight> spot_lights = {};
        std::vector<RenderDataAreaLight> area_lights = {};
        std::vector<RenderDataDirectionalLight> directional_lights = {};
        RenderDataSky sky = {};
        PostProcessing post_processing = {};
        bool has_skybox = false;

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
        RenderDataBuilder(EntityRegistry& registry);
        ~RenderDataBuilder() noexcept = default;

      public:
        RenderDataBuilder(const RenderDataBuilder&) = delete;
        RenderDataBuilder& operator=(const RenderDataBuilder&) = delete;
        RenderDataBuilder(RenderDataBuilder&&) noexcept = default;
        RenderDataBuilder& operator=(RenderDataBuilder&&) noexcept = default;

      public:
        /// @brief
        /// Purpose: Captures the current ECS render data into the render data.
        void build(RenderData& render_data) const;

      private:
        void append_dynamic_meshes(RenderData& render_data) const;
        void append_sky(RenderData& render_data) const;
        void append_static_meshes(RenderData& render_data) const;
        MaterialInstance get_material(Entity& entity) const;

      private:
        std::reference_wrapper<EntityRegistry> _registry;
        MaterialInstance _default_material = MaterialInstance(Handle("Materials/Flat.mat"));
    };
}
