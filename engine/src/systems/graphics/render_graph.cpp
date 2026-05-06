#include "tbx/systems/graphics/render_graph.h"
#include "tbx/systems/ecs/entity.h"

namespace tbx
{
    RenderGraphBuilder::RenderGraphBuilder(EntityRegistry& registry)
        : _registry(registry)
    {
    }

    RenderGraph RenderGraphBuilder::build() const
    {
        auto graph = RenderGraph {};
        append_sky(graph);
        append_dynamic_meshes(graph);
        append_static_meshes(graph);
        return graph;
    }

    void RenderGraphBuilder::append_dynamic_meshes(RenderGraph& graph) const
    {
        _registry.get().for_each_with<DynamicMesh, Transform>(
            [this, &graph](Entity& entity)
            {
                const auto& dynamic_mesh = entity.get_component<DynamicMesh>();
                if (!dynamic_mesh.data)
                    return;

                graph.renderables.push_back(
                    RenderGraphRenderable {
                        .entity_uuid = entity.get_id(),
                        .geometry_source = RenderGraphGeometrySource::DynamicMesh,
                        .dynamic_mesh = dynamic_mesh.data,
                        .material = get_material(entity),
                        .transform = get_world_space_transform(entity),
                    });
            });
    }

    void RenderGraphBuilder::append_sky(RenderGraph& graph) const
    {
        auto found = false;
        _registry.get().for_each_with<Sky>(
            [&graph, &found](Entity& entity)
            {
                if (found)
                    return;

                graph.sky.sky = entity.get_component<Sky>();
                if (entity.has_component<Transform>())
                    graph.sky.transform = get_world_space_transform(entity);
                found = true;
            });
    }

    void RenderGraphBuilder::append_static_meshes(RenderGraph& graph) const
    {
        _registry.get().for_each_with<StaticMesh, Transform>(
            [this, &graph](Entity& entity)
            {
                const auto& static_mesh = entity.get_component<StaticMesh>();
                if (!static_mesh.handle.is_valid())
                    return;

                graph.renderables.push_back(
                    RenderGraphRenderable {
                        .entity_uuid = entity.get_id(),
                        .geometry_source = RenderGraphGeometrySource::StaticMesh,
                        .static_mesh = static_mesh.handle,
                        .material = get_material(entity),
                        .transform = get_world_space_transform(entity),
                    });
            });
    }

    MaterialInstance RenderGraphBuilder::get_material(Entity& entity) const
    {
        if (entity.has_component<MaterialInstance>())
            return entity.get_component<MaterialInstance>();

        return _default_material;
    }
}
