#pragma once
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/ecs/entity_registry.h"
#include "tbx/systems/graphics/light.h"
#include "tbx/systems/graphics/material.h"
#include "tbx/systems/graphics/mesh.h"
#include "tbx/systems/graphics/post_processing.h"
#include "tbx/systems/math/transform.h"
#include "tbx/tbx_api.h"
#include "tbx/types/uuid.h"
#include <functional>
#include <memory>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Identifies where renderable geometry comes from before backend resources are
    /// resolved.
    /// @details
    /// Ownership: Value type. Thread Safety: Safe to copy between threads.
    enum class RenderGraphGeometrySource
    {
        DynamicMesh,
        StaticMesh
    };

    /// @brief
    /// Purpose: Describes one scene renderable captured from ECS for backend-neutral render
    /// preparation.
    /// @details
    /// Ownership: Copies entity, material, static model handle, and transform data by value;
    /// shares runtime mesh data via std::shared_ptr when source is DynamicMesh.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API RenderGraphRenderable
    {
        Uuid entity_uuid = {};
        RenderGraphGeometrySource geometry_source = RenderGraphGeometrySource::DynamicMesh;
        std::shared_ptr<Mesh> dynamic_mesh = {};
        Handle static_mesh = {};
        MaterialInstance material = {};
        Transform transform = {};
        bool is_visible = true;
    };

    /// @brief
    /// Purpose: Describes one point light submitted to a graphics backend.
    /// @details
    /// Ownership: Stores light and transform data by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API RenderGraphPointLight
    {
        Uuid entity_uuid = {};
        PointLight light = {};
        Transform transform = {};
        bool is_visible = true;
    };

    /// @brief
    /// Purpose: Describes one spot light submitted to a graphics backend.
    /// @details
    /// Ownership: Stores light and transform data by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API RenderGraphSpotLight
    {
        Uuid entity_uuid = {};
        SpotLight light = {};
        Transform transform = {};
        bool is_visible = true;
    };

    /// @brief
    /// Purpose: Describes one area light submitted to a graphics backend.
    /// @details
    /// Ownership: Stores light and transform data by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API RenderGraphAreaLight
    {
        Uuid entity_uuid = {};
        AreaLight light = {};
        Transform transform = {};
        bool is_visible = true;
    };

    /// @brief
    /// Purpose: Describes one directional light submitted to a graphics backend.
    /// @details
    /// Ownership: Stores light and transform data by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API RenderGraphDirectionalLight
    {
        Uuid entity_uuid = {};
        DirectionalLight light = {};
        Transform transform = {};
        bool is_visible = true;
    };

    /// @brief
    /// Purpose: Describes the sky submitted to the render graph and the transform used to place it.
    /// @details
    /// Ownership: Stores sky and transform data by value. Defaults to a black sky so every frame
    /// has deterministic sky state even when no Sky component exists in ECS.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API RenderGraphSky
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
    /// Purpose: Stores backend-neutral scene data for one render submission.
    /// @details
    /// Ownership: Owns all submission lists and scene-level render settings by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API RenderGraph
    {
        std::vector<RenderGraphRenderable> renderables = {};
        std::vector<RenderGraphPointLight> point_lights = {};
        std::vector<RenderGraphSpotLight> spot_lights = {};
        std::vector<RenderGraphAreaLight> area_lights = {};
        std::vector<RenderGraphDirectionalLight> directional_lights = {};
        RenderGraphSky sky = {};
        PostProcessing post_processing = {};
    };

    /// @brief
    /// Purpose: Builds a backend-neutral RenderGraph snapshot from the ECS scene.
    /// @details
    /// Ownership: Borrows the entity registry and returns owned render graph snapshots.
    /// Thread Safety: Not thread-safe; callers should synchronize ECS access externally.
    class TBX_API RenderGraphBuilder final
    {
      public:
        RenderGraphBuilder(EntityRegistry& registry);
        ~RenderGraphBuilder() noexcept = default;

      public:
        RenderGraphBuilder(const RenderGraphBuilder&) = delete;
        RenderGraphBuilder& operator=(const RenderGraphBuilder&) = delete;
        RenderGraphBuilder(RenderGraphBuilder&&) noexcept = default;
        RenderGraphBuilder& operator=(RenderGraphBuilder&&) noexcept = default;

      public:
        /// @brief
        /// Purpose: Captures the current ECS render data into a render graph.
        /// @details
        /// Ownership: Returns an owned graph snapshot.
        /// Thread Safety: Not thread-safe; callers should synchronize ECS access externally.
        RenderGraph build() const;

      private:
        void append_dynamic_meshes(RenderGraph& graph) const;
        void append_sky(RenderGraph& graph) const;
        void append_static_meshes(RenderGraph& graph) const;
        MaterialInstance get_material(Entity& entity) const;

      private:
        std::reference_wrapper<EntityRegistry> _registry;
        MaterialInstance _default_material = MaterialInstance(Handle("Materials/Flat.mat"));
    };
}
