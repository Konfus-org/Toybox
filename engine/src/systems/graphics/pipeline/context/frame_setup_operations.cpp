#include "tbx/systems/graphics/pipeline/context/frame_setup_operations.h"
#include "tbx/interfaces/window_manager.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/graphics/pipeline/context/render_data.h"
#include "tbx/systems/graphics/resource_manager.h"
#include "tbx/types/frustum.h"
#include "tbx/types/handle.h"
#include "tbx/types/matrices.h"
#include "tbx/types/mesh_bounds.h"
#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace tbx
{
    struct ViewUniformBlock
    {
        Mat4 view_projection = Mat4(1.0F);
    };

    static RenderOperationDebugInfo make_debug_info(
        const std::string& name,
        const std::string& category)
    {
        auto debug_info = RenderOperationDebugInfo();
        debug_info.debug_name = name;
        debug_info.category = category;
        return debug_info;
    }

    static Size resolve_render_resolution(
        const IWindowManager& window_manager,
        const RenderData& frame_data)
    {
        if (frame_data.requested_resolution.width > 0U
            && frame_data.requested_resolution.height > 0U)
            return frame_data.requested_resolution;
        return window_manager.get_size(frame_data.output_window);
    }

    static float get_max_abs_scale_component(const Vec3& scale)
    {
        return std::max(std::abs(scale.x), std::max(std::abs(scale.y), std::abs(scale.z)));
    }

    static Sphere make_transform_fallback_bounds(const Transform& transform)
    {
        const float fallback_radius = 0.8660254F * get_max_abs_scale_component(transform.scale);
        return Sphere {
            .center = transform.position,
            .radius = fallback_radius,
        };
    }

    static Sphere merge_spheres(const Sphere& left, const Sphere& right)
    {
        const Vec3 center_delta = right.center - left.center;
        const float center_distance = glm::length(center_delta);

        if (left.radius >= (center_distance + right.radius))
            return left;
        if (right.radius >= (center_distance + left.radius))
            return right;

        if (center_distance <= 0.000001F)
        {
            return Sphere {
                .center = left.center,
                .radius = std::max(left.radius, right.radius),
            };
        }

        const float new_radius = (center_distance + left.radius + right.radius) * 0.5F;
        const Vec3 direction = center_delta / center_distance;
        const Vec3 new_center = left.center + (direction * (new_radius - left.radius));
        return Sphere {
            .center = new_center,
            .radius = new_radius,
        };
    }

    static bool within_max_distance_sq(
        const Vec3& camera_position,
        const Vec3& target_position,
        const float max_distance)
    {
        if (max_distance <= 0.0F)
            return true;
        const Vec3 delta = target_position - camera_position;
        const float max_sq = max_distance * max_distance;
        return glm::dot(delta, delta) <= max_sq;
    }

    BuildRenderDataOperation::BuildRenderDataOperation(
        std::weak_ptr<EntityRegistry> entity_registry)
        : _entity_registry(std::move(entity_registry))
    {
    }

    RenderOperationDebugInfo BuildRenderDataOperation::get_debug_info() const
    {
        return make_debug_info("Toybox Build Render Data Operation", "Frame Setup");
    }

    Result BuildRenderDataOperation::prepare(RenderData& render_data)
    {
        auto entity_registry = _entity_registry.lock();
        if (!entity_registry)
            return Result(false, "BuildRenderDataOperation requires EntityRegistry service.");

        render_data = RenderDataBuilder(_entity_registry)
                          .build(
                              render_data.output_window,
                              render_data.requested_resolution,
                              render_data.frame_index,
                              render_data.shadow_map_resolution,
                              render_data.shadow_render_distance,
                              render_data.shadow_softness,
                              render_data.local_light_max_distance,
                              render_data.shadow_caster_max_distance);
        return {};
    }

    Result BuildRenderDataOperation::execute(
        IGraphicsBackend&,
        RenderData&,
        const CancellationToken&)
    {
        return {};
    }

    CullNonVisibleRenderDataItemsOperation::CullNonVisibleRenderDataItemsOperation(
        GraphicsResourceManager& resource_manager)
        : _resource_manager(resource_manager)
    {
    }

    RenderOperationDebugInfo CullNonVisibleRenderDataItemsOperation::get_debug_info() const
    {
        return make_debug_info(
            "Toybox Cull Non-Visible Render Data Items Operation",
            "Frame Setup");
    }

    Result CullNonVisibleRenderDataItemsOperation::prepare(RenderData& render_data)
    {
        auto& renderables = render_data.renderables;
        const auto camera_frustum = render_data.camera.get_frustum(
            render_data.camera_transform.position,
            render_data.camera_transform.rotation);
        auto& resource_manager = _resource_manager.get();

        for (auto& renderable : renderables)
        {
            renderable.is_visible = false;
            auto local_bounds = Sphere {};
            auto has_local_bounds = false;

            if (renderable.geometry_source == RenderDataGeometrySource::DynamicMesh)
            {
                const auto& dynamic_mesh = renderable.dynamic_mesh;
                if (dynamic_mesh)
                {
                    has_local_bounds = dynamic_mesh->bounds.is_valid;
                    local_bounds = dynamic_mesh->bounds.sphere;
                }
            }
            else if (renderable.geometry_source == RenderDataGeometrySource::StaticMesh)
            {
                if (renderable.static_mesh.is_valid())
                {
                    auto model_resource = GraphicsModelResource {};
                    if (const auto result =
                            resource_manager.load_model(renderable.static_mesh, model_resource);
                        result)
                    {
                        auto merged_bounds = Sphere {};
                        auto has_merged_bounds = false;
                        for (const auto& mesh : model_resource.meshes)
                        {
                            if (!mesh.has_local_bounds)
                                continue;
                            merged_bounds = has_merged_bounds
                                                ? merge_spheres(merged_bounds, mesh.local_bounds)
                                                : mesh.local_bounds;
                            has_merged_bounds = true;
                        }

                        has_local_bounds = has_merged_bounds;
                        local_bounds = has_merged_bounds ? merged_bounds : Sphere {};
                    }
                    else
                    {
                        TBX_TRACE_WARNING(
                            "CullNonVisibleRenderDataItemsOperation: could not load static mesh "
                            "GPU metadata for bounds culling ({}). Using transform fallback.",
                            to_string(renderable.static_mesh));
                    }
                }
            }

            if (!has_local_bounds)
            {
                renderable.world_bounds = make_transform_fallback_bounds(renderable.transform);
                renderable.is_visible = camera_frustum.intersects(renderable.world_bounds);
                continue;
            }

            renderable.world_bounds = transform_sphere(local_bounds, renderable.transform);
            renderable.is_visible = camera_frustum.intersects(renderable.world_bounds);
        }

        auto& directional_lights = render_data.directional_lights;
        auto& point_lights = render_data.point_lights;
        auto& spot_lights = render_data.spot_lights;
        auto& area_lights = render_data.area_lights;

        for (auto& light : directional_lights)
            light.is_visible = true;

        const Vec3& camera_position = render_data.camera_position;
        const float local_light_max = render_data.local_light_max_distance;

        for (auto& light : point_lights)
        {
            const float light_range = std::max(light.light.range, 0.0F);
            light.is_visible = light_range > 0.0001F
                               && within_max_distance_sq(
                                   camera_position,
                                   light.transform.position,
                                   local_light_max);
        }

        for (auto& light : spot_lights)
        {
            const float range = std::max(light.light.range, 0.0F);
            light.is_visible = range > 0.0001F
                               && within_max_distance_sq(
                                   camera_position,
                                   light.transform.position,
                                   local_light_max);
        }

        for (auto& light : area_lights)
        {
            const float range = std::max(light.light.range, 0.0F);
            light.is_visible = range > 0.0001F
                               && within_max_distance_sq(
                                   camera_position,
                                   light.transform.position,
                                   local_light_max);
        }

        directional_lights.erase(
            std::remove_if(
                directional_lights.begin(),
                directional_lights.end(),
                [](const RenderDataDirectionalLight& light)
                {
                    return !light.is_visible;
                }),
            directional_lights.end());

        point_lights.erase(
            std::remove_if(
                point_lights.begin(),
                point_lights.end(),
                [](const RenderDataPointLight& light)
                {
                    return !light.is_visible;
                }),
            point_lights.end());

        spot_lights.erase(
            std::remove_if(
                spot_lights.begin(),
                spot_lights.end(),
                [](const RenderDataSpotLight& light)
                {
                    return !light.is_visible;
                }),
            spot_lights.end());

        area_lights.erase(
            std::remove_if(
                area_lights.begin(),
                area_lights.end(),
                [](const RenderDataAreaLight& light)
                {
                    return !light.is_visible;
                }),
            area_lights.end());
        return {};
    }

    Result CullNonVisibleRenderDataItemsOperation::execute(
        IGraphicsBackend&,
        RenderData&,
        const CancellationToken&)
    {
        return {};
    }

    SelectCameraOperation::SelectCameraOperation(
        std::weak_ptr<EntityRegistry> entity_registry,
        std::weak_ptr<IWindowManager> window_manager)
        : _entity_registry(std::move(entity_registry))
        , _window_manager(std::move(window_manager))
    {
    }

    RenderOperationDebugInfo SelectCameraOperation::get_debug_info() const
    {
        return make_debug_info("Toybox Select Camera Operation", "Frame Setup");
    }

    Result SelectCameraOperation::prepare(RenderData& render_data)
    {
        auto window_manager = _window_manager.lock();
        auto entity_registry = _entity_registry.lock();
        if (!window_manager || !entity_registry)
            return Result(
                false,
                "SelectCameraOperation requires EntityRegistry and IWindowManager services.");

        auto& frame_data = render_data;
        frame_data.render_resolution = resolve_render_resolution(*window_manager, frame_data);
        frame_data.viewport = Viewport {
            .position = Vec2(0.0F),
            .dimensions = frame_data.render_resolution,
        };

        frame_data.camera_transform = Transform(Vec3(0.0F, 2.0F, 8.0F));
        frame_data.active_camera_entity_id = {};
        const auto camera_entity = entity_registry->first_with<Camera, Transform>();
        if (camera_entity.get_id().is_valid())
        {
            frame_data.camera = camera_entity.get_component<Camera>();
            frame_data.camera_transform = get_world_space_transform(camera_entity);
            frame_data.active_camera_entity_id = camera_entity.get_id();
        }

        const float aspect = frame_data.render_resolution.height == 0U
                                 ? 1.0F
                                 : static_cast<float>(frame_data.render_resolution.width)
                                       / static_cast<float>(frame_data.render_resolution.height);
        frame_data.camera.set_aspect(aspect);
        frame_data.camera_position = frame_data.camera_transform.position;
        frame_data.view_projection = frame_data.camera.get_view_projection_matrix(
            frame_data.camera_transform.position,
            frame_data.camera_transform.rotation);
        return {};
    }

    Result SelectCameraOperation::execute(IGraphicsBackend&, RenderData&, const CancellationToken&)
    {
        return {};
    }

    BeginFrameOperation::BeginFrameOperation(std::weak_ptr<IGraphicsBackend> backend)
        : _backend(std::move(backend))
    {
    }

    RenderOperationDebugInfo BeginFrameOperation::get_debug_info() const
    {
        return make_debug_info("Toybox Begin Frame Operation", "Frame Setup");
    }

    Result BeginFrameOperation::prepare(RenderData& render_data)
    {
        auto backend = _backend.lock();
        if (!backend)
            return Result(false, "BeginFrameOperation requires IGraphicsBackend service.");

        auto& frame_data = render_data;
        if (frame_data.frame_started)
            return {};

        if (const auto result = backend->begin_frame(
                GraphicsFrameInfo {
                    .output_window = frame_data.output_window,
                    .render_resolution = frame_data.render_resolution,
                    .output_resolution = frame_data.render_resolution,
                });
            !result)
            return result;

        frame_data.frame_started = true;
        return {};
    }

    Result BeginFrameOperation::execute(IGraphicsBackend&, RenderData&, const CancellationToken&)
    {
        return {};
    }

    UpdateViewUniformsOperation::UpdateViewUniformsOperation(
        std::weak_ptr<IGraphicsBackend> backend)
        : _backend(std::move(backend))
    {
    }

    RenderOperationDebugInfo UpdateViewUniformsOperation::get_debug_info() const
    {
        return make_debug_info("Toybox Update View Uniforms Operation", "Frame Setup");
    }

    Result UpdateViewUniformsOperation::prepare(RenderData& render_data)
    {
        auto backend = _backend.lock();
        if (!backend)
            return Result(false, "UpdateViewUniformsOperation requires IGraphicsBackend service.");

        auto& frame_data = render_data;
        const auto block = ViewUniformBlock {.view_projection = frame_data.view_projection};
        const auto data_size = static_cast<uint64>(sizeof(ViewUniformBlock));

        if (!_buffer.is_valid())
        {
            if (const auto result = backend->upload_buffer(
                    GraphicsBufferDesc {
                        .usage = GraphicsBufferUsage::UNIFORM,
                        .size = data_size,
                        .is_dynamic = true,
                        .debug_name = "Toybox View Uniforms",
                    },
                    &block,
                    data_size,
                    _buffer);
                !result)
                return result;
        }
        else if (
            const auto result = backend->update_buffer(_buffer, &block, data_size, 0U); !result)
        {
            return result;
        }

        frame_data.view_uniform_buffer = _buffer;
        return {};
    }

    Result UpdateViewUniformsOperation::execute(
        IGraphicsBackend&,
        RenderData&,
        const CancellationToken&)
    {
        return {};
    }

    void UpdateViewUniformsOperation::release(IGraphicsBackend& backend)
    {
        if (_buffer.is_valid())
        {
            backend.unload(_buffer);
            _buffer = {};
        }
    }

}
