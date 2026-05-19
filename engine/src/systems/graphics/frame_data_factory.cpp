#include "tbx/systems/graphics/frame_data_factory.h"
#include "tbx/systems/assets/fallbacks.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/types/components/camera.h"
#include "tbx/types/components/material_instance.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/material.h"
#include "tbx/types/matrices.h"
#include "tbx/types/vertex.h"
#include <algorithm>
#include <string>
#include <utility>

namespace tbx
{
    // TODO: this and the other binding consts in resource manager should be defined in the a
    // shader_bindings.h for re-use and easy management/maintainance
    namespace detail
    {
        constexpr uint32 TBX_BINDING_FRAME_DATA = 0U;
        constexpr uint32 TBX_BINDING_CAMERA_DATA = 1U;
        constexpr uint32 TBX_BINDING_OBJECT_DATA = 2U;
        constexpr uint32 TBX_BINDING_MATERIAL_DATA = 3U;
        constexpr uint32 TBX_BINDING_LIGHT_DATA = 20U;

        struct alignas(16) FrameShaderData
        {
            float time = 0.0F;
            float delta_time = 0.0F;
            Vec2 viewport_size = Vec2(1.0F, 1.0F);
        };

        struct alignas(16) CameraShaderData
        {
            Mat4 view = Mat4(1.0F);
            Mat4 projection = Mat4(1.0F);
            Mat4 view_projection = Mat4(1.0F);
            Mat4 inverse_view = Mat4(1.0F);
            Mat4 inverse_projection = Mat4(1.0F);
            Vec4 world_position = Vec4(0.0F, 0.0F, 0.0F, 1.0F);
        };

        struct alignas(16) ObjectShaderData
        {
            Mat4 model = Mat4(1.0F);
            Mat4 normal_matrix = Mat4(1.0F);
        };

        struct alignas(16) LightShaderData
        {
            Vec4 ambient_color = Vec4(0.2F, 0.2F, 0.2F, 1.0F);
            IVec4 light_meta = IVec4(0, 0, 0, 0);
        };

        static Mat4 build_normal_matrix(const Mat4& model_matrix)
        {
            const Mat3 normal_matrix3 = inverse_transpose(Mat3(model_matrix));
            auto normal_matrix = Mat4(1.0F);
            normal_matrix[0] = Vec4(normal_matrix3[0], 0.0F);
            normal_matrix[1] = Vec4(normal_matrix3[1], 0.0F);
            normal_matrix[2] = Vec4(normal_matrix3[2], 0.0F);
            return normal_matrix;
        }

        static bool upload_uniform_buffer(
            GraphicsResourceManager& resource_manager,
            const std::string& debug_name,
            const void* data,
            const uint64 byte_size,
            Uuid& out_buffer)
        {
            return resource_manager.upload(
                GraphicsBufferDesc {
                    .usage = GraphicsBufferUsage::UNIFORM,
                    .size = byte_size,
                    .is_dynamic = true,
                    .debug_name = debug_name,
                },
                data,
                byte_size,
                out_buffer);
        }
    }

    FrameDataFactory::FrameDataFactory(std::weak_ptr<EntityRegistry> entity_registry)
        : _entity_registry(std::move(entity_registry))
    {
    }

    // TODO: rename to RenderPassFactory and its only job should be creating the render passes.
    Result FrameDataFactory::create(
        GraphicsResourceManager& resource_manager,
        FrameData& out_frame_data) const
    {
        // TODO: separate out into a upload_frame_globals in rendering
        const auto entity_registry = _entity_registry.lock();
            if (!entity_registry)
                return Result(false, "Frame data factory: required scene service unavailable.");

            if (out_frame_data.render_resolution.width == 0U
                || out_frame_data.render_resolution.height == 0U)
                out_frame_data.render_resolution = Size {1U, 1U};
            if (out_frame_data.view.viewport.dimensions.width == 0U
                || out_frame_data.view.viewport.dimensions.height == 0U)
            {
                out_frame_data.view.viewport = Viewport {
                    .position = Vec2(0.0F),
                    .dimensions = out_frame_data.render_resolution,
                };
            }

            auto active_camera = out_frame_data.view.camera;
            auto active_camera_transform = Transform {};
            for (auto& entity : entity_registry->get_with<Camera, Transform>())
            {
                active_camera = entity.get_component<Camera>();
                active_camera_transform = entity.get_component<Transform>();
                break;
            }

            active_camera.set_aspect(out_frame_data.render_resolution.get_aspect_ratio());
            out_frame_data.view.camera = active_camera;

            const Mat4 view_matrix = active_camera.get_view_matrix(
                active_camera_transform.position,
                active_camera_transform.rotation);
            const Mat4 projection_matrix = active_camera.get_projection_matrix();
            const Mat4 view_projection_matrix = projection_matrix * view_matrix;

            const auto frame_shader_data = detail::FrameShaderData {
                .time = 0.0F,
                .delta_time = 0.0F,
                .viewport_size = Vec2(
                    static_cast<float>(out_frame_data.render_resolution.width),
                    static_cast<float>(out_frame_data.render_resolution.height)),
            };
            const auto camera_shader_data = detail::CameraShaderData {
                .view = view_matrix,
                .projection = projection_matrix,
                .view_projection = view_projection_matrix,
                .inverse_view = inverse(view_matrix),
                .inverse_projection = inverse(projection_matrix),
                .world_position = Vec4(active_camera_transform.position, 1.0F),
            };
            const auto light_shader_data = detail::LightShaderData {};

            auto frame_uniform_buffer = Uuid {};
            if (const auto result = detail::upload_uniform_buffer(
                    resource_manager,
                    "Frame Shader Data",
                    &frame_shader_data,
                    static_cast<uint64>(sizeof(frame_shader_data)),
                    frame_uniform_buffer);
                !result)
            {
                return Result(false, "Frame data factory: failed to upload frame shader buffer.");
            }

            auto camera_uniform_buffer = Uuid {};
            if (const auto result = detail::upload_uniform_buffer(
                    resource_manager,
                    "Camera Shader Data",
                    &camera_shader_data,
                    static_cast<uint64>(sizeof(camera_shader_data)),
                    camera_uniform_buffer);
                !result)
            {
                return Result(false, "Frame data factory: failed to upload camera shader data.");
            }

            auto light_uniform_buffer = Uuid {};
            if (const auto result = detail::upload_uniform_buffer(
                    resource_manager,
                    "Light Shader Data",
                    &light_shader_data,
                    static_cast<uint64>(sizeof(light_shader_data)),
                    light_uniform_buffer);
                !result)
            {
                return Result(false, "Frame data factory: failed to upload light shader data.");
            }

            const auto build_common_uniform_bindings =
                [&](const Uuid& object_uniform_buffer, const Uuid& material_uniform_buffer)
            {
                return std::vector<GraphicsResourceBinding> {
                    GraphicsResourceBinding {
                        .slot = detail::TBX_BINDING_FRAME_DATA,
                        .resource = frame_uniform_buffer,
                    },
                    GraphicsResourceBinding {
                        .slot = detail::TBX_BINDING_CAMERA_DATA,
                        .resource = camera_uniform_buffer,
                    },
                    GraphicsResourceBinding {
                        .slot = detail::TBX_BINDING_OBJECT_DATA,
                        .resource = object_uniform_buffer,
                    },
                    GraphicsResourceBinding {
                        .slot = detail::TBX_BINDING_MATERIAL_DATA,
                        .resource = material_uniform_buffer,
                    },
                    GraphicsResourceBinding {
                        .slot = detail::TBX_BINDING_LIGHT_DATA,
                        .resource = light_uniform_buffer,
                    },
                };
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

        const auto fallback_material = MaterialInstance {};
        bool has_static_geometry = false;
        bool has_sky_geometry = false;
        uint32 max_dynamic_index_count = 0U;

        for (auto& entity : entity_registry->get_with<DynamicMesh, Transform>())
        {
            const auto& mesh_component = entity.get_component<DynamicMesh>();
            if (!mesh_component.data)
                continue;

            const auto& mesh = *mesh_component.data;
            max_dynamic_index_count =
                std::max(max_dynamic_index_count, static_cast<uint32>(mesh.indices.size()));
            if (mesh.vertices.empty() || mesh.indices.empty())
                continue;

            auto model_resource = GraphicsModelResource {};
            const auto model_handle =
                Handle(std::string("Toybox/DynamicMesh/") + to_string(entity.get_id()));
            if (const auto result = resource_manager.upload(model_handle, mesh, model_resource);
                !result)
            {
                return Result(false, "Frame data factory: failed to upload model mesh.");
            }

            const auto* material_instance = entity.has_component<MaterialInstance>()
                                                ? &entity.get_component<MaterialInstance>()
                                                : nullptr;
            auto material_resource = GraphicsMaterialDrawResource {};
            if (const auto result = resource_manager.upload(
                    material_instance ? *material_instance : fallback_material,
                    material_resource);
                !result)
            {
                return Result(false, "Frame data factory: failed to upload material.");
            }

            const auto model_matrix = build_transform_matrix(entity.get_component<Transform>());
            const auto object_shader_data = detail::ObjectShaderData {
                .model = model_matrix,
                .normal_matrix = detail::build_normal_matrix(model_matrix),
            };
            auto object_uniform_buffer = Uuid {};
            if (const auto result = detail::upload_uniform_buffer(
                    resource_manager,
                    "Object Shader Data",
                    &object_shader_data,
                    static_cast<uint64>(sizeof(object_shader_data)),
                    object_uniform_buffer);
                !result)
            {
                return Result(false, "Frame data factory: failed to upload object shader data.");
            }

            auto material_uniform_buffer = Uuid {};
            if (const auto result = detail::upload_uniform_buffer(
                    resource_manager,
                    "Material Shader Data",
                    material_resource.uniform_data.data(),
                    material_resource.uniform_data.byte_size(),
                    material_uniform_buffer);
                !result)
            {
                return Result(false, "Frame data factory: failed to upload material uniforms.");
            }

            const auto uniform_bindings =
                build_common_uniform_bindings(object_uniform_buffer, material_uniform_buffer);

            for (const auto& mesh_resource : model_resource.meshes)
            {
                if (!mesh_resource.vertex_buffer.is_valid()
                    || !mesh_resource.index_buffer.is_valid() || mesh_resource.index_count == 0U)
                {
                    continue;
                }

                opaque_pass.indexed_draws.push_back(
                    GraphicsIndexedDrawCommand {
                        .pipeline = material_resource.pipeline,
                        .index_buffer = mesh_resource.index_buffer,
                        .index_type = GraphicsIndexType::UINT32,
                        .vertex_buffers = {GraphicsResourceBinding {
                            .slot = 0U,
                            .resource = mesh_resource.vertex_buffer,
                        }},
                        .uniform_buffers = uniform_bindings,
                        .textures = material_resource.textures,
                        .draw =
                            GraphicsDrawIndexedDesc {
                                .primitive_type = GraphicsPrimitiveType::TRIANGLES,
                                .index_type = GraphicsIndexType::UINT32,
                                .index_count = mesh_resource.index_count,
                                .index_offset = 0U,
                                .vertex_offset = 0,
                                .instance_count = 1U,
                                .first_instance = 0U,
                            },
                    });
            }
        }

        for (auto& entity : entity_registry->get_with<StaticMesh, Transform>())
        {
            has_static_geometry = true;
            const auto& static_mesh = entity.get_component<StaticMesh>();
            if (!static_mesh.handle.is_valid())
                continue;

            auto model_resource = GraphicsModelResource {};
            if (const auto result = resource_manager.upload(
                    static_mesh.handle,
                    ModelLoadParameters {},
                    model_resource);
                !result)
            {
                return Result(false, "Frame data factory: failed to upload static mesh.");
            }

            const auto* material_instance = entity.has_component<MaterialInstance>()
                                                ? &entity.get_component<MaterialInstance>()
                                                : nullptr;
            auto material_resource = GraphicsMaterialDrawResource {};
            if (const auto result = resource_manager.upload(
                    material_instance ? *material_instance : fallback_material,
                    material_resource);
                !result)
            {
                return Result(false, "Frame data factory: failed to upload material.");
            }

            const auto model_matrix = build_transform_matrix(entity.get_component<Transform>());
            const auto object_shader_data = detail::ObjectShaderData {
                .model = model_matrix,
                .normal_matrix = detail::build_normal_matrix(model_matrix),
            };
            auto object_uniform_buffer = Uuid {};
            if (const auto result = detail::upload_uniform_buffer(
                    resource_manager,
                    "Object Shader Data",
                    &object_shader_data,
                    static_cast<uint64>(sizeof(object_shader_data)),
                    object_uniform_buffer);
                !result)
            {
                return Result(false, "Frame data factory: failed to upload object shader data.");
            }

            auto material_uniform_buffer = Uuid {};
            if (const auto result = detail::upload_uniform_buffer(
                    resource_manager,
                    "Material Shader Data",
                    material_resource.uniform_data.data(),
                    material_resource.uniform_data.byte_size(),
                    material_uniform_buffer);
                !result)
            {
                return Result(false, "Frame data factory: failed to upload material uniforms.");
            }

            const auto uniform_bindings =
                build_common_uniform_bindings(object_uniform_buffer, material_uniform_buffer);

            for (const auto& mesh_resource : model_resource.meshes)
            {
                if (!mesh_resource.vertex_buffer.is_valid()
                    || !mesh_resource.index_buffer.is_valid() || mesh_resource.index_count == 0U)
                {
                    continue;
                }

                opaque_pass.indexed_draws.push_back(
                    GraphicsIndexedDrawCommand {
                        .pipeline = material_resource.pipeline,
                        .index_buffer = mesh_resource.index_buffer,
                        .index_type = GraphicsIndexType::UINT32,
                        .vertex_buffers = {GraphicsResourceBinding {
                            .slot = 0U,
                            .resource = mesh_resource.vertex_buffer,
                        }},
                        .uniform_buffers = uniform_bindings,
                        .textures = material_resource.textures,
                        .draw =
                            GraphicsDrawIndexedDesc {
                                .primitive_type = GraphicsPrimitiveType::TRIANGLES,
                                .index_type = GraphicsIndexType::UINT32,
                                .index_count = mesh_resource.index_count,
                                .index_offset = 0U,
                                .vertex_offset = 0,
                                .instance_count = 1U,
                                .first_instance = 0U,
                            },
                    });
            }
        }

        for (auto& entity : entity_registry->get_with<Sky, Transform>())
        {
            has_sky_geometry = true;
            auto& sky = entity.get_component<Sky>();
            auto material_resource = GraphicsMaterialDrawResource {};
            if (const auto result = resource_manager.upload(sky.material, material_resource);
                !result)
            {
                return Result(false, "Frame data factory: failed to upload sky material.");
            }

            auto model_resource = GraphicsModelResource {};
            if (const auto result =
                    resource_manager.upload(Handle("Toybox/SkyDome"), sky_dome, model_resource);
                !result)
            {
                return Result(false, "Frame data factory: failed to upload sky dome mesh.");
            }

            const auto model_matrix = build_transform_matrix(entity.get_component<Transform>());
            const auto object_shader_data = detail::ObjectShaderData {
                .model = model_matrix,
                .normal_matrix = detail::build_normal_matrix(model_matrix),
            };
            auto object_uniform_buffer = Uuid {};
            if (const auto result = detail::upload_uniform_buffer(
                    resource_manager,
                    "Sky Object Shader Data",
                    &object_shader_data,
                    static_cast<uint64>(sizeof(object_shader_data)),
                    object_uniform_buffer);
                !result)
            {
                return Result(false, "Frame data factory: failed to upload sky object data.");
            }

            auto material_uniform_buffer = Uuid {};
            if (const auto result = detail::upload_uniform_buffer(
                    resource_manager,
                    "Sky Material Shader Data",
                    material_resource.uniform_data.data(),
                    material_resource.uniform_data.byte_size(),
                    material_uniform_buffer);
                !result)
            {
                return Result(false, "Frame data factory: failed to upload sky material uniforms.");
            }

            const auto uniform_bindings =
                build_common_uniform_bindings(object_uniform_buffer, material_uniform_buffer);

            for (const auto& mesh_resource : model_resource.meshes)
            {
                skybox_pass.indexed_draws.push_back(
                    GraphicsIndexedDrawCommand {
                        .pipeline = material_resource.pipeline,
                        .index_buffer = mesh_resource.index_buffer,
                        .index_type = GraphicsIndexType::UINT32,
                        .vertex_buffers = {GraphicsResourceBinding {
                            .slot = 0U,
                            .resource = mesh_resource.vertex_buffer,
                        }},
                        .uniform_buffers = uniform_bindings,
                        .textures = material_resource.textures,
                        .draw =
                            GraphicsDrawIndexedDesc {
                                .primitive_type = GraphicsPrimitiveType::TRIANGLES,
                                .index_type = GraphicsIndexType::UINT32,
                                .index_count = mesh_resource.index_count,
                                .index_offset = 0U,
                                .vertex_offset = 0,
                                .instance_count = 1U,
                                .first_instance = 0U,
                            },
                    });
            }
        }

        const bool has_skybox_pass =
            !skybox_pass.draws.empty() || !skybox_pass.indexed_draws.empty();
        if (has_skybox_pass)
            out_frame_data.passes.push_back(std::move(skybox_pass));

        if (!opaque_pass.draws.empty() || !opaque_pass.indexed_draws.empty())
        {
            if (has_skybox_pass)
                opaque_pass.pass.clear_flags = GraphicsClearFlags::DEPTH;
            out_frame_data.passes.push_back(std::move(opaque_pass));
        }

        if (!alpha_cutout_pass.draws.empty() || !alpha_cutout_pass.indexed_draws.empty())
            out_frame_data.passes.push_back(std::move(alpha_cutout_pass));

        const bool should_add_lighting_pass =
            has_static_geometry || has_sky_geometry || max_dynamic_index_count > 3U;
        if (should_add_lighting_pass)
        {
            auto lighting_pass = RenderPass {
                .pass =
                    GraphicsPassDesc {
                        .clear_color = Color::BLACK,
                        .clear_depth = 1.0F,
                        .clear_flags = GraphicsClearFlags::NONE,
                        .debug_name = "Toybox Lighting Pass",
                    },
            };
            out_frame_data.passes.push_back(std::move(lighting_pass));
        }

        if (!transparent_pass.draws.empty() || !transparent_pass.indexed_draws.empty())
            out_frame_data.passes.push_back(std::move(transparent_pass));

        if (!post_process_pass.draws.empty() || !post_process_pass.indexed_draws.empty())
            out_frame_data.passes.push_back(std::move(post_process_pass));

        return {};
    }
}
