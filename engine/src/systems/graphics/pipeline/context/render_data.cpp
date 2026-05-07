#include "tbx/systems/graphics/pipeline/context/render_data.h"
#include "tbx/systems/ecs/entity.h"

namespace tbx
{
    RenderData::RenderData(FrameData frame_data)
        : frame(frame_data)
    {
    }

    RenderDataBuilder::RenderDataBuilder(EntityRegistry& registry)
        : _registry(registry)
    {
    }

    void RenderDataBuilder::build(RenderData& render_data) const
    {
        render_data.renderables.clear();
        render_data.point_lights.clear();
        render_data.spot_lights.clear();
        render_data.area_lights.clear();
        render_data.directional_lights.clear();
        render_data.sky = RenderDataSky {};
        render_data.post_processing = PostProcessing {};
        render_data.has_skybox = false;
        render_data.skybox_commands.clear();
        render_data.opaque_commands.clear();
        render_data.alpha_cutout_commands.clear();
        render_data.transparent_commands.clear();

        append_sky(render_data);
        append_dynamic_meshes(render_data);
        append_static_meshes(render_data);
    }

    void RenderDataBuilder::append_dynamic_meshes(RenderData& render_data) const
    {
        _registry.get().for_each_with<DynamicMesh, Transform>(
            [this, &render_data](Entity& entity)
            {
                const auto& dynamic_mesh = entity.get_component<DynamicMesh>();
                if (!dynamic_mesh.data)
                    return;

                render_data.renderables.push_back(
                    RenderDataRenderable {
                        .entity_uuid = entity.get_id(),
                        .geometry_source = RenderDataGeometrySource::DynamicMesh,
                        .dynamic_mesh = dynamic_mesh.data,
                        .material = get_material(entity),
                        .transform = get_world_space_transform(entity),
                    });
            });
    }

    void RenderDataBuilder::append_sky(RenderData& render_data) const
    {
        auto found = false;
        _registry.get().for_each_with<Sky>(
            [&render_data, &found](Entity& entity)
            {
                if (found)
                    return;

                render_data.sky.sky = entity.get_component<Sky>();
                if (entity.has_component<Transform>())
                    render_data.sky.transform = get_world_space_transform(entity);
                found = true;
            });
    }

    void RenderDataBuilder::append_static_meshes(RenderData& render_data) const
    {
        _registry.get().for_each_with<StaticMesh, Transform>(
            [this, &render_data](Entity& entity)
            {
                const auto& static_mesh = entity.get_component<StaticMesh>();
                if (!static_mesh.handle.is_valid())
                    return;

                render_data.renderables.push_back(
                    RenderDataRenderable {
                        .entity_uuid = entity.get_id(),
                        .geometry_source = RenderDataGeometrySource::StaticMesh,
                        .static_mesh = static_mesh.handle,
                        .material = get_material(entity),
                        .transform = get_world_space_transform(entity),
                    });
            });
    }

    MaterialInstance RenderDataBuilder::get_material(Entity& entity) const
    {
        if (entity.has_component<MaterialInstance>())
            return entity.get_component<MaterialInstance>();

        return _default_material;
    }
}
