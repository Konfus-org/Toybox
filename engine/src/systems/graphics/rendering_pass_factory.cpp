#include "tbx/systems/graphics/rendering_pass_factory.h"
#include "tbx/systems/assets/fallbacks.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/types/components/camera.h"
#include "tbx/types/components/light.h"
#include "tbx/types/components/material_instance.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/matrices.h"
#include "tbx/types/trig.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>

namespace tbx::detail
{
    static void append_material_parameter_value(
        std::ostringstream& stream,
        const MaterialParameterData& data)
    {
        stream << data.index() << ":";
        std::visit(
            [&stream](const auto& value)
            {
                using TValue = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<TValue, bool>)
                    stream << (value ? 1 : 0);
                else if constexpr (std::is_same_v<TValue, int>)
                    stream << value;
                else if constexpr (std::is_same_v<TValue, float>)
                    stream << value;
                else if constexpr (std::is_same_v<TValue, double>)
                    stream << value;
                else if constexpr (std::is_same_v<TValue, Vec2>)
                    stream << value.x << "," << value.y;
                else if constexpr (std::is_same_v<TValue, Vec3>)
                    stream << value.x << "," << value.y << "," << value.z;
                else if constexpr (std::is_same_v<TValue, Vec4>)
                    stream << value.x << "," << value.y << "," << value.z << "," << value.w;
                else if constexpr (std::is_same_v<TValue, Color>)
                    stream << value.r << "," << value.g << "," << value.b << "," << value.a;
                else if constexpr (std::is_same_v<TValue, Mat3>)
                    stream << value[0].x << "," << value[0].y << "," << value[0].z << ","
                           << value[1].x << "," << value[1].y << "," << value[1].z << ","
                           << value[2].x << "," << value[2].y << "," << value[2].z;
                else if constexpr (std::is_same_v<TValue, Mat4>)
                    stream << value[0].x << "," << value[0].y << "," << value[0].z << ","
                           << value[0].w << "," << value[1].x << "," << value[1].y << ","
                           << value[1].z << "," << value[1].w << "," << value[2].x << ","
                           << value[2].y << "," << value[2].z << "," << value[2].w << ","
                           << value[3].x << "," << value[3].y << "," << value[3].z << ","
                           << value[3].w;
            },
            data);
    }

    static std::string make_material_batch_key(const MaterialInstance& material)
    {
        auto stream = std::ostringstream {};
        stream << to_string(material.get_handle());
        stream << "|config=" << (material.has_config_override_enabled() ? 1 : 0);
        if (material.has_config_override_enabled())
        {
            stream << "," << (material.config.is_depth_test_enabled ? 1 : 0);
            stream << "," << (material.config.is_depth_write_enabled ? 1 : 0);
            stream << "," << (material.config.is_two_sided ? 1 : 0);
            stream << "," << (material.config.is_cullable ? 1 : 0);
            stream << "," << static_cast<int>(material.config.depth_function);
            stream << "," << static_cast<int>(material.config.blend_mode);
            stream << "," << static_cast<int>(material.config.shadow_mode);
        }

        for (const auto& parameter : material.param_overrides)
        {
            stream << "|p:" << parameter.name << "=";
            append_material_parameter_value(stream, parameter.data);
        }
        for (const auto& texture : material.texture_overrides)
            stream << "|t:" << texture.name << "=" << to_string(texture.texture);

        return stream.str();
    }

    static std::string make_dynamic_batch_key(
        const DynamicMeshData& mesh_data,
        const MaterialInstance& material)
    {
        return std::to_string(reinterpret_cast<std::uintptr_t>(&mesh_data)) + "|"
               + make_material_batch_key(material);
    }

    static Vec3 make_light_color(const Light& light)
    {
        return Vec3(light.color.r, light.color.g, light.color.b);
    }

    static void append_shader_light(
        LightShaderData& light_data,
        const ShaderLightData& shader_light)
    {
        const int32 light_count = light_data.light_meta.x;
        if (light_count < 0 || static_cast<uint32>(light_count) >= MAX_LIGHTS)
            return;

        light_data.lights[static_cast<size>(light_count)] = shader_light;
        light_data.light_meta.x = light_count + 1;
    }

    static LightShaderData build_light_shader_data(EntityRegistry& entity_registry)
    {
        auto light_data = LightShaderData();
        auto ambient_color = Vec3(0.0F);
        auto has_directional_ambient = false;

        for (auto& entity : entity_registry.get_with<DirectionalLight, Transform>())
        {
            const auto& light = entity.get_component<DirectionalLight>();
            const Transform transform = get_world_space_transform(entity);
            const Vec3 direction =
                normalize_or_zero(transform.rotation * Vec3(0.0F, 0.0F, -1.0F));
            const Vec3 color = make_light_color(light);

            ambient_color += color * light.ambient;
            has_directional_ambient = true;

            append_shader_light(
                light_data,
                ShaderLightData {
                    .position_type = Vec4(0.0F, 0.0F, 0.0F, SHADER_LIGHT_TYPE_DIRECTIONAL),
                    .direction_range = Vec4(direction, 0.0F),
                    .color_intensity = Vec4(color, light.intensity),
                    .params = Vec4(0.0F, 0.0F, -1.0F, 0.0F),
                });
        }

        for (auto& entity : entity_registry.get_with<PointLight, Transform>())
        {
            const auto& light = entity.get_component<PointLight>();
            const Transform transform = get_world_space_transform(entity);
            const Vec3 color = make_light_color(light);

            append_shader_light(
                light_data,
                ShaderLightData {
                    .position_type = Vec4(transform.position, SHADER_LIGHT_TYPE_POINT),
                    .direction_range = Vec4(0.0F, 0.0F, 0.0F, light.range),
                    .color_intensity = Vec4(color, light.intensity),
                    .params = Vec4(0.0F, 0.0F, -1.0F, 0.0F),
                });
        }

        for (auto& entity : entity_registry.get_with<SpotLight, Transform>())
        {
            const auto& light = entity.get_component<SpotLight>();
            const Transform transform = get_world_space_transform(entity);
            const Vec3 direction =
                normalize_or_zero(transform.rotation * Vec3(0.0F, 0.0F, -1.0F));
            const Vec3 color = make_light_color(light);

            append_shader_light(
                light_data,
                ShaderLightData {
                    .position_type = Vec4(transform.position, SHADER_LIGHT_TYPE_SPOT),
                    .direction_range = Vec4(direction, light.range),
                    .color_intensity = Vec4(color, light.intensity),
                    .params = Vec4(
                        angle_to_cosine(light.inner_angle),
                        angle_to_cosine(light.outer_angle),
                        -1.0F,
                        0.0F),
                });
        }

        if (has_directional_ambient)
            light_data.ambient_color = Vec4(ambient_color, 1.0F);

        return light_data;
    }

    struct DynamicMeshRenderBatch
    {
        std::shared_ptr<DynamicMeshData> mesh_data = {};
        MaterialInstance material = {};
        std::vector<RenderingDrawInstanceData> instances = {};
    };
}

namespace tbx
{
    RenderingPassFactory::RenderingPassFactory(
        std::weak_ptr<IGraphicsBackend> backend,
        std::weak_ptr<EntityRegistry> entity_registry,
        std::weak_ptr<AssetManager> asset_manager,
        std::weak_ptr<IWindowManager> window_manager,
        const Size configured_resolution)
        : _entity_registry(std::move(entity_registry))
        , _window_manager(std::move(window_manager))
        , _configured_resolution(configured_resolution)
        , _resource_uploader(std::move(backend), std::move(asset_manager))
        , _sky_dome_mesh(std::make_shared<Mesh>(sky_dome))
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

        out_render_passes.reserve(out_render_passes.size() + 5U);

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
        if (!frame_uniform_buffers[0].resource.is_valid()
            || !frame_uniform_buffers[1].resource.is_valid()
            || !frame_uniform_buffers[2].resource.is_valid())
        {
            return Result(false, "Frame pipeline factory failed: frame uniform upload failed.");
        }

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
        auto has_static_geometry = false;
        auto has_sky_geometry = false;
        auto max_dynamic_index_count = uint32 {};

        auto dynamic_batches = std::unordered_map<std::string, detail::DynamicMeshRenderBatch> {};
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
            const std::string batch_key = detail::make_dynamic_batch_key(*mesh_data, material);
            auto& batch = dynamic_batches[batch_key];
            if (!batch.mesh_data)
            {
                batch.mesh_data = mesh_data;
                batch.material = material;
            }
            batch.instances.push_back(
                RenderingDrawInstanceData {
                    .model_matrix = model_matrix,
                    .normal_matrix = normal(model_matrix),
                });
        }

        for (const auto& [batch_key, batch] : dynamic_batches)
        {
            const auto result = _draw_command_factory.create(
                frame_index,
                frame_uniform_buffers,
                RenderingDrawCommandInput {
                    .type = RenderingDrawCommandInputType::DYNAMIC,
                    .handle = Handle(std::string("Toybox/DynamicMesh/") + batch_key),
                    .instance_key = std::string("Toybox/DynamicMeshBatch/") + batch_key,
                    .dynamic_mesh = batch.mesh_data,
                    .material = batch.material,
                    .instances = batch.instances,
                },
                _resource_uploader,
                resource_tracker,
                opaque_pass.indexed_draws);
            if (!result)
                return result;
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
            const auto model_matrix = build_transform_matrix(get_world_space_transform(entity));
            const auto result = _draw_command_factory.create(
                frame_index,
                frame_uniform_buffers,
                RenderingDrawCommandInput {
                    .type = RenderingDrawCommandInputType::STATIC,
                    .handle = static_mesh.handle,
                    .instance_key = std::string("Toybox/Entity/") + to_string(entity.get_id()),
                    .material = material_instance ? *material_instance : fallback_material,
                    .model_matrix = model_matrix,
                    .normal_matrix = normal(model_matrix),
                },
                _resource_uploader,
                resource_tracker,
                opaque_pass.indexed_draws);
            if (!result)
                return result;
        }

        for (auto& entity : entity_registry->get_with<Sky, Transform>())
        {
            has_sky_geometry = true;
            const auto& sky = entity.get_component<Sky>();
            const auto model_matrix = build_transform_matrix(get_world_space_transform(entity));
            const auto result = _draw_command_factory.create(
                frame_index,
                frame_uniform_buffers,
                RenderingDrawCommandInput {
                    .type = RenderingDrawCommandInputType::STATIC_RUNTIME,
                    .handle = Handle("Toybox/SkyDome"),
                    .instance_key = std::string("Toybox/Entity/") + to_string(entity.get_id()),
                    .runtime_mesh = _sky_dome_mesh,
                    .material = sky.material,
                    .model_matrix = model_matrix,
                    .normal_matrix = normal(model_matrix),
                },
                _resource_uploader,
                resource_tracker,
                skybox_pass.indexed_draws);
            if (!result)
                return result;
        }

        const bool has_skybox_pass =
            !skybox_pass.draws.empty() || !skybox_pass.indexed_draws.empty();
        if (has_skybox_pass)
            out_render_passes.push_back(std::move(skybox_pass));

        if (!opaque_pass.draws.empty() || !opaque_pass.indexed_draws.empty())
        {
            if (has_skybox_pass)
                opaque_pass.pass.clear_flags = GraphicsClearFlags::DEPTH;
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
        _light_shader_data = detail::build_light_shader_data(entity_registry);
        out_render_target = render_target;
        out_view = RenderView {
            .camera = active_camera,
            .viewport = Viewport {.position = Vec2(0.0F), .dimensions = render_resolution},
        };

        return {};
    }
}
