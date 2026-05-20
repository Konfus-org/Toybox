#include "tbx/systems/graphics/rendering_pass_factory.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/graphics/internal/rendering_pass_factory_internal.h"
#include "tbx/systems/graphics/shader_bindings.h"
#include "tbx/types/components/camera.h"
#include "tbx/types/components/light.h"
#include "tbx/types/components/material_instance.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/matrices.h"
#include "tbx/types/trig.h"
#include "tbx/utils/hash.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

namespace tbx
{
    RenderingPassFactory::RenderingPassFactory(
        std::weak_ptr<IGraphicsBackend> backend,
        std::weak_ptr<EntityRegistry> entity_registry,
        std::weak_ptr<AssetManager> asset_manager,
        std::weak_ptr<IWindowManager> window_manager,
        const GraphicsSettings& settings)
        : _entity_registry(std::move(entity_registry))
        , _window_manager(std::move(window_manager))
        , _configured_resolution(settings.resolution.value)
        , _shadow_map_resolution(settings.shadow_map_resolution.value)
        , _shadow_render_distance(settings.shadow_render_distance.value)
        , _shadow_softness(settings.shadow_softness.value)
        , _local_light_max_distance(settings.local_light_max_distance.value)
        , _shadow_caster_max_distance(settings.shadow_caster_max_distance.value)
        , _resource_uploader(std::move(backend), std::move(asset_manager))
    {
    }

    Result RenderingPassFactory::create(
        const uint64 frame_index,
        RenderingResourceTracker& resource_tracker,
        std::vector<RenderPass>& out_render_passes)
    {
        const auto entity_registry = _entity_registry.lock();
        if (!entity_registry)
            return Result(false, "Frame pipeline factory failed: scene service unavailable.");

        out_render_passes.reserve(out_render_passes.size() + 6U);

        const auto frame_uniform_buffers = std::array<GraphicsResourceBinding, 3U> {
            _resource_uploader.upload_uniform_buffer(
                resource_tracker,
                BINDING_FRAME_DATA,
                "Frame Shader Data",
                "Toybox/Uniforms/Frame",
                frame_index,
                &_frame_shader_data,
                static_cast<uint64>(sizeof(_frame_shader_data))),
            _resource_uploader.upload_uniform_buffer(
                resource_tracker,
                BINDING_CAMERA_DATA,
                "Camera Shader Data",
                "Toybox/Uniforms/Camera",
                frame_index,
                &_camera_shader_data,
                static_cast<uint64>(sizeof(_camera_shader_data))),
            _resource_uploader.upload_uniform_buffer(
                resource_tracker,
                BINDING_LIGHT_DATA,
                "Light Shader Data",
                "Toybox/Uniforms/Light",
                frame_index,
                &_light_shader_data,
                static_cast<uint64>(sizeof(_light_shader_data))),
        };
        const GraphicsResourceBinding shadow_pass_uniform_buffer =
            _resource_uploader.upload_uniform_buffer(
                resource_tracker,
                BINDING_SHADOW_PASS_DATA,
                "Shadow Pass Shader Data",
                "Toybox/Uniforms/ShadowPass",
                frame_index,
                &_shadow_pass_shader_data,
                static_cast<uint64>(sizeof(_shadow_pass_shader_data)));
        if (!frame_uniform_buffers[0].resource.is_valid()
            || !frame_uniform_buffers[1].resource.is_valid()
            || !frame_uniform_buffers[2].resource.is_valid()
            || !shadow_pass_uniform_buffer.resource.is_valid())
        {
            return Result(false, "Frame pipeline factory failed: frame uniform upload failed.");
        }

        const bool has_shadowed_light = _light_shader_data.light_meta.y > 0;
        const uint32 shadow_resolution = std::max(_shadow_map_resolution, 1U);
        const GraphicsResourceBinding shadow_map =
            has_shadowed_light
                ? _resource_uploader.upload_texture(
                      resource_tracker,
                      BINDING_SHADOW_MAP,
                      std::string("Toybox/ShadowMap/") + std::to_string(shadow_resolution),
                      GraphicsTextureDesc {
                          .usage = GraphicsTextureUsage::SAMPLED_DEPTH_STENCIL,
                          .format = GraphicsTextureFormat::DEPTH32_FLOAT,
                          .size = Size {shadow_resolution, shadow_resolution},
                          .mip_count = 1U,
                          .array_layer_count = 1U,
                          .debug_name = "Toybox Directional "
                                        "Shadow Map",
                      })
                : GraphicsResourceBinding {.slot = BINDING_SHADOW_MAP};
        if (has_shadowed_light && !shadow_map.resource.is_valid())
            return Result(false, "Frame pipeline factory failed: shadow map upload failed.");

        const auto shadow_uniform_buffers = std::array<GraphicsResourceBinding, 3U> {
            frame_uniform_buffers[0],
            frame_uniform_buffers[1],
            shadow_pass_uniform_buffer,
        };
        auto shadow_pass = RenderPass {
            .pass =
                GraphicsPassDesc {
                    .depth_stencil_target = shadow_map.resource,
                    .clear_depth = 1.0F,
                    .clear_flags = GraphicsClearFlags::DEPTH,
                    .debug_name = "Toybox Directional Shadow Pass",
                },
        };
        auto skybox_pass = RenderPass {
            .pass =
                GraphicsPassDesc {
                    .clear_color = Color::BLACK,
                    .clear_depth = 1.0F,
                    .clear_flags = GraphicsClearFlags::COLOR_DEPTH,
                    .debug_name = "Toybox Skybox Pass",
                },
        };
        auto opaque_pass = RenderPass {
            .pass =
                GraphicsPassDesc {
                    .clear_color = Color::BLACK,
                    .clear_depth = 1.0F,
                    .clear_flags = GraphicsClearFlags::COLOR_DEPTH,
                    .debug_name = "Toybox Opaque Scene Pass",
                },
        };
        auto alpha_cutout_pass = RenderPass {
            .pass =
                GraphicsPassDesc {
                    .clear_depth = 1.0F,
                    .clear_flags = GraphicsClearFlags::NONE,
                    .debug_name = "Toybox Alpha Cutout Scene Pass",
                },
        };
        auto transparent_pass = RenderPass {
            .pass =
                GraphicsPassDesc {
                    .clear_depth = 1.0F,
                    .clear_flags = GraphicsClearFlags::NONE,
                    .debug_name = "Toybox Transparent Forward Pass",
                },
        };
        auto post_process_pass = RenderPass {
            .pass =
                GraphicsPassDesc {
                    .clear_flags = GraphicsClearFlags::NONE,
                    .debug_name = "Toybox Post Process Pass",
                },
        };

        const auto fallback_material = MaterialInstance(PbrMaterial::HANDLE);
        const auto shadow_material = MaterialInstance(Handle("Materials/DirectionalShadowMap.mat"));
        const uint64 shadow_material_key = hash(shadow_material);
        const Vec3 camera_position = Vec3(_camera_shader_data.world_position);
        auto has_static_geometry = false;
        auto has_sky_geometry = false;
        auto max_dynamic_index_count = uint32 {};
        auto material_uploads = std::unordered_map<uint64, RenderingMaterialUploadData> {};
        auto material_uniform_buffers = std::unordered_map<uint64, GraphicsResourceBinding> {};
        auto opaque_batches = internal::RenderBatchCollection {};
        auto shadow_batches = internal::RenderBatchCollection {};

        for (auto& entity : entity_registry->get_with<DynamicMesh, Transform>())
        {
            const auto& mesh_component = entity.get_component<DynamicMesh>();
            const auto mesh_data = mesh_component.get_data();
            if (!mesh_data)
                continue;

            const auto& mesh = mesh_component.get_mesh();
            max_dynamic_index_count =
                std::max(max_dynamic_index_count, static_cast<uint32>(mesh.indices.size()));
            if (mesh.vertices.empty() || mesh.indices.empty())
                continue;

            const auto* material_instance = entity.has_component<MaterialInstance>()
                                                ? &entity.get_component<MaterialInstance>()
                                                : nullptr;
            const auto model_matrix = build_transform_matrix(get_world_space_transform(entity));
            const auto material = material_instance ? *material_instance : fallback_material;
            const uint64 material_key = hash(material);
            const auto mesh_source = RenderingMeshSourceType::DYNAMIC_RUNTIME_MESH;
            const Uuid mesh_id =
                Uuid(static_cast<uint32>(reinterpret_cast<std::uintptr_t>(mesh_data.get())));
            const auto instance = RenderingDrawInstanceData {
                .model_matrix = model_matrix,
                .normal_matrix = normal(model_matrix),
            };
            internal::append_render_batch_instance(
                opaque_batches,
                internal::hash_render_batch(mesh_source, mesh_id, mesh_data.get(), material_key),
                mesh_source,
                material_key,
                Handle("Toybox/DynamicMesh"),
                mesh_data,
                material,
                instance);

            if (has_shadowed_light
                && internal::should_material_cast_shadows(
                    material,
                    get_world_space_transform(entity).position,
                    camera_position,
                    _shadow_caster_max_distance))
            {
                internal::append_render_batch_instance(
                    shadow_batches,
                    internal::hash_render_batch(
                        mesh_source,
                        mesh_id,
                        mesh_data.get(),
                        shadow_material_key),
                    mesh_source,
                    shadow_material_key,
                    Handle("Toybox/DynamicMeshShadow"),
                    mesh_data,
                    shadow_material,
                    instance);
            }
        }

        for (auto& entity : entity_registry->get_with<StaticMesh, Transform>())
        {
            has_static_geometry = true;
            const auto& static_mesh = entity.get_component<StaticMesh>();
            if (!static_mesh.handle.is_valid())
                continue;

            const auto* material_instance = entity.has_component<MaterialInstance>()
                                                ? &entity.get_component<MaterialInstance>()
                                                : nullptr;
            const Transform transform = get_world_space_transform(entity);
            const auto model_matrix = build_transform_matrix(transform);
            const auto material = material_instance ? *material_instance : fallback_material;
            const uint64 material_key = hash(material);
            const auto instance = RenderingDrawInstanceData {
                .model_matrix = model_matrix,
                .normal_matrix = normal(model_matrix),
            };
            internal::append_render_batch_instance(
                opaque_batches,
                internal::hash_render_batch(
                    RenderingMeshSourceType::MODEL_ASSET,
                    static_mesh.handle.get_id(),
                    nullptr,
                    material_key),
                RenderingMeshSourceType::MODEL_ASSET,
                material_key,
                static_mesh.handle,
                {},
                material,
                instance);

            if (has_shadowed_light
                && internal::should_material_cast_shadows(
                    material,
                    transform.position,
                    camera_position,
                    _shadow_caster_max_distance))
            {
                internal::append_render_batch_instance(
                    shadow_batches,
                    internal::hash_render_batch(
                        RenderingMeshSourceType::MODEL_ASSET,
                        static_mesh.handle.get_id(),
                        nullptr,
                        shadow_material_key),
                    RenderingMeshSourceType::MODEL_ASSET,
                    shadow_material_key,
                    static_mesh.handle,
                    {},
                    shadow_material,
                    instance);
            }
        }

        for (const auto& [batch_key, batch] : shadow_batches)
        {
            const auto result = _draw_command_factory.create(
                frame_index,
                shadow_uniform_buffers,
                RenderingDrawBatchInput {
                    .mesh_source = batch.mesh_source,
                    .mesh_handle = batch.mesh_handle,
                    .batch_key = batch_key,
                    .debug_name = internal::make_batch_debug_name(batch_key, "Toybox/ShadowBatch/"),
                    .dynamic_mesh = batch.dynamic_mesh,
                    .material = batch.material,
                    .material_key = batch.material_key,
                    .instances = batch.instances,
                },
                _resource_uploader,
                resource_tracker,
                material_uploads,
                material_uniform_buffers,
                shadow_pass.indexed_draws);
            if (!result)
                return result;
        }

        for (const auto& [batch_key, batch] : opaque_batches)
        {
            const auto result = _draw_command_factory.create(
                frame_index,
                frame_uniform_buffers,
                RenderingDrawBatchInput {
                    .mesh_source = batch.mesh_source,
                    .mesh_handle = batch.mesh_handle,
                    .batch_key = batch_key,
                    .debug_name = internal::make_batch_debug_name(batch_key, "Toybox/RenderBatch/"),
                    .dynamic_mesh = batch.dynamic_mesh,
                    .material = batch.material,
                    .material_key = batch.material_key,
                    .instances = batch.instances,
                },
                _resource_uploader,
                resource_tracker,
                material_uploads,
                material_uniform_buffers,
                opaque_pass.indexed_draws);
            if (!result)
                return result;
        }

        const auto sky_dome_handle = Handle("Toybox/SkyDome");
        for (auto& entity : entity_registry->get_with<Sky, Transform>())
        {
            has_sky_geometry = true;
            if (!_sky_dome_mesh && !_resource_uploader.has_static_runtime_mesh(sky_dome_handle))
                _sky_dome_mesh = std::make_shared<Mesh>(make_sky_dome());

            const auto& sky = entity.get_component<Sky>();
            const auto model_matrix = build_transform_matrix(get_world_space_transform(entity));
            const uint64 material_key = hash(sky.material);
            const auto result = _draw_command_factory.create(
                frame_index,
                frame_uniform_buffers,
                RenderingDrawBatchInput {
                    .mesh_source = RenderingMeshSourceType::STATIC_RUNTIME_MESH,
                    .mesh_handle = sky_dome_handle,
                    .batch_key = hash(entity.get_id(), material_key),
                    .debug_name = std::string("Toybox/Sky/Entity/") + to_string(entity.get_id()),
                    .runtime_mesh = _sky_dome_mesh,
                    .material = sky.material,
                    .material_key = material_key,
                    .instances =
                        {
                            RenderingDrawInstanceData {
                                .model_matrix = model_matrix,
                                .normal_matrix = normal(model_matrix),
                            },
                        },
                },
                _resource_uploader,
                resource_tracker,
                material_uploads,
                material_uniform_buffers,
                skybox_pass.indexed_draws);
            if (!result)
                return result;

            _sky_dome_mesh.reset();
        }

        const bool has_skybox_pass =
            !skybox_pass.draws.empty() || !skybox_pass.indexed_draws.empty();
        if (!shadow_pass.draws.empty() || !shadow_pass.indexed_draws.empty())
            out_render_passes.push_back(std::move(shadow_pass));

        if (has_skybox_pass)
            out_render_passes.push_back(std::move(skybox_pass));

        if (!opaque_pass.draws.empty() || !opaque_pass.indexed_draws.empty())
        {
            if (has_skybox_pass)
                opaque_pass.pass.clear_flags = GraphicsClearFlags::DEPTH;
            for (auto& draw : opaque_pass.draws)
            {
                draw.uniform_buffers.push_back(shadow_pass_uniform_buffer);
                if (shadow_map.resource.is_valid())
                    draw.textures.push_back(shadow_map);
            }
            for (auto& draw : opaque_pass.indexed_draws)
            {
                draw.uniform_buffers.push_back(shadow_pass_uniform_buffer);
                if (shadow_map.resource.is_valid())
                    draw.textures.push_back(shadow_map);
            }
            out_render_passes.push_back(std::move(opaque_pass));
        }

        if (!alpha_cutout_pass.draws.empty() || !alpha_cutout_pass.indexed_draws.empty())
            out_render_passes.push_back(std::move(alpha_cutout_pass));

        const bool should_add_lighting_pass =
            has_static_geometry || has_sky_geometry || max_dynamic_index_count > 3U;
        if (should_add_lighting_pass)
        {
            out_render_passes.push_back(
                RenderPass {
                    .pass =
                        GraphicsPassDesc {
                            .clear_color = Color::BLACK,
                            .clear_depth = 1.0F,
                            .clear_flags = GraphicsClearFlags::NONE,
                            .debug_name = "Toybox Lighting Pass",
                        },
                });
        }

        if (!transparent_pass.draws.empty() || !transparent_pass.indexed_draws.empty())
            out_render_passes.push_back(std::move(transparent_pass));

        if (!post_process_pass.draws.empty() || !post_process_pass.indexed_draws.empty())
            out_render_passes.push_back(std::move(post_process_pass));

        return {};
    }

    Result RenderingPassFactory::build_frame_data(
        const DeltaTime delta_time,
        RenderTarget& out_render_target,
        RenderView& out_view)
    {
        const auto entity_registry = _entity_registry.lock();
        if (!entity_registry)
            return Result(false, "Frame pipeline factory failed: scene service unavailable.");

        const auto window_manager = _window_manager.lock();
        if (!window_manager)
            return Result(false, "Frame pipeline factory failed: window manager unavailable.");

        return build_frame_data(
            *entity_registry,
            *window_manager,
            delta_time,
            out_render_target,
            out_view);
    }

    void RenderingPassFactory::discard_cached_resource(const Uuid& resource)
    {
        _resource_uploader.discard_cached_resource(resource);
    }

    Result RenderingPassFactory::build_frame_data(
        EntityRegistry& entity_registry,
        const IWindowManager& window_manager,
        const DeltaTime delta_time,
        RenderTarget& out_render_target,
        RenderView& out_view)
    {
        if (!window_manager.has_main_window())
            return Result(false, "Frame pipeline factory failed: no main output window.");

        auto active_camera = Camera();
        auto active_camera_transform = Transform();
        for (auto& entity : entity_registry.get_with<Camera, Transform>())
        {
            active_camera = entity.get_component<Camera>();
            active_camera_transform = get_world_space_transform(entity);
            break;
        }

        auto render_target = window_manager.get_main_window();
        auto cam_target = active_camera.get_render_target();
        if (cam_target.is_valid())
            render_target = cam_target;

        auto target_resolution = window_manager.get_size(render_target);
        if (target_resolution.width == 0U || target_resolution.height == 0U)
            target_resolution = Size {1U, 1U};

        auto render_resolution = _configured_resolution;
        if (render_resolution.width == 0U || render_resolution.height == 0U)
            render_resolution = target_resolution;

        active_camera.set_aspect(render_resolution.get_aspect_ratio());
        const Mat4 view_matrix = active_camera.get_view_matrix(
            active_camera_transform.position,
            active_camera_transform.rotation);
        const Mat4 projection_matrix = active_camera.get_projection_matrix();
        const Mat4 view_projection_matrix = projection_matrix * view_matrix;

        _frame_shader_data = FrameShaderData {
            .time = 0.0F,
            .delta_time = static_cast<float>(delta_time.seconds),
            .viewport_size = Vec2(
                static_cast<float>(render_resolution.width),
                static_cast<float>(render_resolution.height)),
        };
        _camera_shader_data = CameraShaderData {
            .view = view_matrix,
            .projection = projection_matrix,
            .view_projection = view_projection_matrix,
            .inverse_view = inverse(view_matrix),
            .inverse_projection = inverse(projection_matrix),
            .world_position = Vec4(active_camera_transform.position, 1.0F),
        };
        _light_shader_data = internal::build_light_shader_data(
            entity_registry,
            active_camera_transform.position,
            _local_light_max_distance);
        _shadow_pass_shader_data = internal::build_shadow_shader_data(
            entity_registry,
            active_camera_transform.position,
            _shadow_render_distance,
            _shadow_softness,
            _local_light_max_distance);
        out_render_target = render_target;
        out_view = RenderView {
            .camera = active_camera,
            .viewport = Viewport {.position = Vec2(0.0F), .dimensions = render_resolution},
        };

        return {};
    }
}
