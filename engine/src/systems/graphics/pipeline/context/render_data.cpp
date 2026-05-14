#include "tbx/systems/graphics/pipeline/context/render_data.h"
#include "tbx/systems/ecs/entity.h"
#include <utility>

namespace tbx
{
    RenderDataBuilder::RenderDataBuilder(std::weak_ptr<EntityRegistry> registry)
        : _registry(std::move(registry))
    {
    }

    RenderData RenderDataBuilder::build(
        const Window& output_window,
        const Size& requested_resolution,
        const uint64 frame_index,
        const uint32 shadow_map_resolution,
        const float shadow_render_distance,
        const float shadow_softness,
        const float local_light_max_distance,
        const float shadow_caster_max_distance) const
    {
        auto render_data = RenderData {};
        render_data.output_window = output_window;
        render_data.requested_resolution = requested_resolution;
        render_data.frame_index = frame_index;
        render_data.shadow_map_resolution = shadow_map_resolution;
        render_data.shadow_render_distance = shadow_render_distance;
        render_data.shadow_softness = shadow_softness;
        render_data.local_light_max_distance = local_light_max_distance;
        render_data.shadow_caster_max_distance = shadow_caster_max_distance;

        append_sky(render_data);
        append_lights(render_data);
        append_dynamic_meshes(render_data);
        append_static_meshes(render_data);
        append_post_processing(render_data);
        return render_data;
    }

    void RenderDataBuilder::append_dynamic_meshes(RenderData& render_data) const
    {
        auto registry = _registry.lock();
        if (!registry)
            return;

        registry->for_each_with<DynamicMesh, Transform>(
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

    void RenderDataBuilder::append_lights(RenderData& render_data) const
    {
        auto registry = _registry.lock();
        if (!registry)
            return;

        registry->for_each_with<PointLight, Transform>(
            [&render_data](Entity& entity)
            {
                render_data.point_lights.push_back(
                    RenderDataPointLight {
                        .entity_uuid = entity.get_id(),
                        .light = entity.get_component<PointLight>(),
                        .transform = get_world_space_transform(entity),
                    });
            });

        registry->for_each_with<SpotLight, Transform>(
            [&render_data](Entity& entity)
            {
                render_data.spot_lights.push_back(
                    RenderDataSpotLight {
                        .entity_uuid = entity.get_id(),
                        .light = entity.get_component<SpotLight>(),
                        .transform = get_world_space_transform(entity),
                    });
            });

        registry->for_each_with<AreaLight, Transform>(
            [&render_data](Entity& entity)
            {
                render_data.area_lights.push_back(
                    RenderDataAreaLight {
                        .entity_uuid = entity.get_id(),
                        .light = entity.get_component<AreaLight>(),
                        .transform = get_world_space_transform(entity),
                    });
            });

        registry->for_each_with<DirectionalLight, Transform>(
            [&render_data](Entity& entity)
            {
                render_data.directional_lights.push_back(
                    RenderDataDirectionalLight {
                        .entity_uuid = entity.get_id(),
                        .light = entity.get_component<DirectionalLight>(),
                        .transform = get_world_space_transform(entity),
                    });
            });
    }

    void RenderDataBuilder::append_sky(RenderData& render_data) const
    {
        auto registry = _registry.lock();
        if (!registry)
            return;

        const auto sky_entity = registry->first_with<Sky>();
        if (!sky_entity.get_id().is_valid())
            return;

        render_data.sky.sky = sky_entity.get_component<Sky>();
        if (sky_entity.has_component<Transform>())
            render_data.sky.transform = get_world_space_transform(sky_entity);
    }

    void RenderDataBuilder::append_post_processing(RenderData& render_data) const
    {
        auto registry = _registry.lock();
        if (!registry)
            return;

        const auto post_processing_entity = registry->first_with<PostProcessing>();
        if (!post_processing_entity.get_id().is_valid())
            return;

        render_data.post_processing = post_processing_entity.get_component<PostProcessing>();
    }

    void RenderDataBuilder::append_static_meshes(RenderData& render_data) const
    {
        auto registry = _registry.lock();
        if (!registry)
            return;

        registry->for_each_with<StaticMesh, Transform>(
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
