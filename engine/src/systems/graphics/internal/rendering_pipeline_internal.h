#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/interfaces/window_manager.h"
#include "tbx/systems/assets/builtin_assets.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/ecs/entity_registry.h"
#include "tbx/systems/graphics/render_pass.h"
#include "tbx/systems/graphics/resource_manager.h"
#include "tbx/systems/graphics/shader_bindings.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/types/components/camera.h"
#include "tbx/types/components/light.h"
#include "tbx/types/components/material_instance.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/components/sky.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/matrices.h"
#include "tbx/types/render_target.h"
#include "tbx/types/trig.h"
#include "tbx/types/viewport.h"
#include "tbx/utils/hash.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace tbx::internal
{
    struct GBuffer
    {
        GraphicsResourceBinding albedo = {};
        GraphicsResourceBinding normal = {};
        GraphicsResourceBinding material = {};
        GraphicsResourceBinding emissive = {};
        GraphicsResourceBinding depth = {};
        GraphicsResourceBinding final_color = {};
    };

    struct RenderCamera
    {
        Mat4 view = Mat4(1.0F);
        Vec3 position = Vec3(0.0F);
        Mat4 projection = Mat4(1.0F);
        Mat4 view_projection = Mat4(1.0F);
        Mat4 inverse_view = Mat4(1.0F);
        Mat4 inverse_projection = Mat4(1.0F);
    };

    struct RenderLight
    {
        uint type = 0U;
        Color color = Color::WHITE;
        float intensity = 0.0F;
        float inner_cone = 0.0F;
        float outer_cone = 0.0F;
        int32 shadow_index = -1;
        uint32 shadow_layer_count = 0U;
        Vec3 position = Vec3(0.0F);
        Vec3 direction = Vec3(0.0F);
        float range = 0.0F;
    };

    struct ShadowCascades
    {
        uint32 count = 0U;
        GraphicsResourceBinding map = {};
        std::vector<GraphicsResourceBinding> buffers = {};
    };

    struct RenderMesh
    {
        using Data = std::variant<StaticMesh, DynamicMesh>;

        Handle handle = {};
        Data data = StaticMesh();
    };

    struct RenderMeshInstance
    {
        Mat4 model_matrix = Mat4(1.0F);
        Mat4 normal_matrix = Mat4(1.0F);
    };

    struct RenderBatch
    {
        Uuid pipeline = {};
        RenderMesh mesh = {};
        MaterialInstance material = {};
        std::vector<RenderMeshInstance> instances = {};
    };

    struct FrameData
    {
        uint index = 0U;
        float time = 0.0F;
        float delta_time = 0.0F;
        Size resolution = {};
        Viewport viewport = {};
        RenderTarget target = {};
        RenderCamera camera = {};
        GBuffer g_buffer = {};
    };

    struct RenderLighting
    {
        std::vector<RenderLight> lights = {};
        Vec3 ambient_color_sum = Vec3(0.0F);
        float ambient_intensity_sum = 0.0F;
        uint32 directional_light_count = 0U;
        uint32 shadow_layer_count = 0U;
    };

    struct RenderDrawData
    {
        using BatchMap = std::unordered_map<uint64, RenderBatch>;

        Sky sky = {};

        RenderLighting lighting = {};
        ShadowCascades shadows = {};

        BatchMap opaque_batches = {};
        BatchMap transparent_batches = {};
        BatchMap shadow_batches = {};

        Color clear_color = Color::BLACK;
    };

    static int32 reserve_shadow_index(
        uint32& shadow_layer_count,
        const bool casts_shadows,
        const uint32 layer_count)
    {
        if (!casts_shadows || layer_count == 0U || shadow_layer_count + layer_count > MAX_LIGHTS)
        {
            return -1;
        }

        const int32 result = static_cast<int32>(shadow_layer_count);
        shadow_layer_count += layer_count;
        return result;
    }

    static MaterialConfig resolve_draw_material_config(
        AssetManager& asset_manager,
        const MaterialInstance& material)
    {
        if (!material.get_handle().is_valid())
            return MaterialConfig();

        if (material.has_config_override_enabled())
            return material.overrides.config;

        const auto loaded_material =
            asset_manager.load<Material>(material.get_handle(), MaterialLoadParameters());

        return loaded_material ? loaded_material->config : MaterialConfig();
    }

    static void append_light(RenderDrawData& draw_data, const RenderLight& render_light)
    {
        if (draw_data.lighting.lights.size() < MAX_LIGHTS)
            draw_data.lighting.lights.push_back(render_light);
    }

    static void append_batch(
        RenderDrawData::BatchMap& batches,
        const uint64 key,
        const RenderMesh& mesh,
        const MaterialInstance& material,
        const RenderMeshInstance& instance)
    {
        auto& batch = batches
                          .try_emplace(
                              key,
                              RenderBatch {
                                  .mesh = mesh,
                                  .material = material,
                              })
                          .first->second;
        batch.instances.push_back(instance);
    }

    static uint64 make_batch_hash(
        const Uuid& mesh_id,
        const DynamicMeshData* dynamic_mesh,
        const uint64 material_key)
    {
        uint64 result = hash(mesh_id, TBX_FNV1A_OFFSET_BASIS);
        result = hash(static_cast<uint64>(reinterpret_cast<std::uintptr_t>(dynamic_mesh)), result);
        result = hash(material_key, result);
        return result == 0U ? 1U : result;
    }

    static Transform get_optional_transform(Entity& entity)
    {
        // Components may omit Transform; identity keeps those renderables deterministic.
        if (entity.has_component<Transform>())
            return get_world_space_transform(entity);

        return Transform();
    }

    static bool is_within_local_light_range(
        const Vec3& position,
        const Vec3& camera_position,
        const float max_distance)
    {
        return max_distance <= 0.0F || distance(position, camera_position) <= max_distance;
    }

    static LightingShaderData make_light_shader_data(
        const RenderDrawData& draw_data,
        const Vec3& ambient_color_sum,
        const float ambient_intensity_sum,
        const uint directional_light_count)
    {
        auto light_data = LightingShaderData();
        light_data.light_meta.x = static_cast<int32>(draw_data.lighting.lights.size());
        light_data.light_meta.y = static_cast<int32>(draw_data.shadows.count);

        if (directional_light_count > 0U)
        {
            const Vec3 ambient_color =
                ambient_color_sum * (1.0F / static_cast<float>(directional_light_count));
            light_data.ambient_color = Vec4(ambient_color * ambient_intensity_sum, 1.0F);
        }

        for (uint light_index = 0U;
             light_index < static_cast<uint>(draw_data.lighting.lights.size());
             ++light_index)
        {
            const auto& light = draw_data.lighting.lights[static_cast<size>(light_index)];
            light_data.lights[light_index] = ShaderLightData {
                .position_type = Vec4(light.position, static_cast<float>(light.type)),
                .direction_range = Vec4(light.direction, light.range),
                .color_intensity =
                    Vec4(light.color.r, light.color.g, light.color.b, light.intensity),
                .params = Vec4(
                    light.inner_cone,
                    light.outer_cone,
                    static_cast<float>(light.shadow_index),
                    static_cast<float>(light.shadow_layer_count)),
            };
        }

        return light_data;
    }

    static bool has_texture_slot(
        const std::vector<GraphicsResourceBinding>& textures,
        const uint32 slot)
    {
        return std::any_of(
            textures.begin(),
            textures.end(),
            [slot](const GraphicsResourceBinding& texture)
            {
                return texture.slot == slot && texture.resource.is_valid();
            });
    }

    static void append_fallback_texture(
        RenderingResourceManager& resource_manager,
        const uint32 binding_id,
        std::vector<GraphicsResourceBinding>& textures)
    {
        const auto slot = resolve_shader_texture_slot(binding_id);
        if (!slot.has_value() || has_texture_slot(textures, *slot))
            return;

        const auto texture = resource_manager.upload_fallback_texture(binding_id);
        if (texture.resource.is_valid())
            textures.push_back(texture);
    }

    static void append_pbr_fallback_textures(
        RenderingResourceManager& resource_manager,
        RenderingMaterialUploadData& material_upload)
    {
        append_fallback_texture(resource_manager, PARAM_ALBEDO_MAP, material_upload.textures);
        append_fallback_texture(resource_manager, PARAM_NORMAL_MAP, material_upload.textures);
        append_fallback_texture(
            resource_manager,
            PARAM_METALLIC_ROUGHNESS_MAP,
            material_upload.textures);
        append_fallback_texture(resource_manager, PARAM_AO_MAP, material_upload.textures);
        append_fallback_texture(resource_manager, PARAM_EMISSIVE_MAP, material_upload.textures);
    }

    static Result upload_batch_material(
        RenderingResourceManager& resource_manager,
        const MaterialInstance& material,
        RenderingMaterialUploadData& out_material)
    {
        const Result result = material.get_handle().is_valid()
                                  ? resource_manager.upload_material(material, out_material)
                                  : resource_manager.upload_fallback_material(out_material);
        if (result)
            append_pbr_fallback_textures(resource_manager, out_material);

        return result;
    }

    static Result upload_batch_meshes(
        RenderingResourceManager& resource_manager,
        const RenderMesh& mesh,
        std::vector<RenderingMeshUploadData>& out_meshes)
    {
        auto result = Result();
        if (std::holds_alternative<StaticMesh>(mesh.data))
        {
            result = mesh.handle.is_valid() ? resource_manager.upload_model(mesh.handle, out_meshes)
                                            : resource_manager.upload_fallback_mesh(out_meshes);
        }
        else
        {
            const auto& dynamic_mesh = std::get<DynamicMesh>(mesh.data);
            const auto mesh_data = dynamic_mesh.get_data();
            if (mesh_data && !dynamic_mesh.get_mesh().vertices.empty()
                && !dynamic_mesh.get_mesh().indices.empty())
            {
                auto uploaded_mesh = RenderingMeshUploadData();
                result = resource_manager.upload_dynamic_mesh(mesh_data, uploaded_mesh);
                if (result)
                    out_meshes.push_back(uploaded_mesh);
            }
            else
            {
                result = resource_manager.upload_fallback_mesh(out_meshes);
            }
        }

        if (result && out_meshes.empty())
            result = resource_manager.upload_fallback_mesh(out_meshes);

        return result;
    }

    static Result append_batch_draws(
        RenderingResourceManager& resource_manager,
        const uint64 frame_index,
        const GraphicsResourceBinding& frame_uniform,
        const GraphicsResourceBinding& camera_uniform,
        const GraphicsResourceBinding& light_uniform,
        const RenderDrawData::BatchMap& batches,
        RenderPass& render_pass)
    {
        for (const auto& [batch_key, batch] : batches)
        {
            if (batch.instances.empty())
                continue;

            auto material_upload = RenderingMaterialUploadData();
            auto result = upload_batch_material(resource_manager, batch.material, material_upload);
            if (!result)
                return result;

            // Object and instance data are uploaded together here so the batch stays CPU-friendly
            // until it is converted into the backend draw contract.
            const auto object_data = ObjectShaderData {
                .model = batch.instances.front().model_matrix,
                .normal_matrix = batch.instances.front().normal_matrix,
            };
            const auto object_uniform = resource_manager.upload_uniform_buffer(
                BINDING_OBJECT_DATA,
                "Object Shader Data",
                std::string("Toybox/Uniforms/Object/") + std::to_string(batch_key),
                frame_index,
                &object_data,
                static_cast<uint64>(sizeof(object_data)));
            const auto material_uniform = resource_manager.upload_uniform_buffer(
                BINDING_MATERIAL_DATA,
                "Material Shader Data",
                std::string("Toybox/Uniforms/Material/") + std::to_string(hash(batch.material)),
                frame_index,
                material_upload.uniform_values.data(),
                static_cast<uint64>(material_upload.uniform_values.size() * sizeof(Vec4)));
            const auto instance_buffer = resource_manager.upload_instance_buffer(
                std::string("Toybox/Instances/") + std::to_string(batch_key),
                frame_index,
                batch.instances.data(),
                static_cast<uint64>(batch.instances.size() * sizeof(RenderMeshInstance)));
            if (!material_upload.pipeline.is_valid() || !object_uniform.resource.is_valid()
                || !material_uniform.resource.is_valid() || !instance_buffer.resource.is_valid())
            {
                return Result(false, "Rendering pipeline failed: draw upload failed.");
            }

            auto uploaded_meshes = std::vector<RenderingMeshUploadData>();
            result = upload_batch_meshes(resource_manager, batch.mesh, uploaded_meshes);
            if (!result)
                return result;

            for (const auto& uploaded_mesh : uploaded_meshes)
            {
                render_pass.indexed_draws.push_back(
                    GraphicsIndexedDrawCommand {
                        .pipeline = material_upload.pipeline,
                        .index_buffer = uploaded_mesh.index_buffer,
                        .index_type = GraphicsIndexType::UINT32,
                        .vertex_buffers =
                            {
                                GraphicsResourceBinding {
                                    .slot = VERTEX_BUFFER_SLOT_MESH,
                                    .resource = uploaded_mesh.vertex_buffer,
                                },
                                instance_buffer,
                            },
                        .uniform_buffers =
                            {
                                frame_uniform,
                                camera_uniform,
                                light_uniform,
                                object_uniform,
                                material_uniform,
                            },
                        .textures = material_upload.textures,
                        .draw =
                            GraphicsDrawIndexedDesc {
                                .index_type = GraphicsIndexType::UINT32,
                                .index_count = uploaded_mesh.index_count,
                                .instance_count = static_cast<uint32>(batch.instances.size()),
                            },
                    });
            }
        }

        return {};
    }

    static std::vector<RenderPass> create_passes(
        const uint frame_index,
        const FrameData& frame,
        const GBuffer& gbuffer,
        const RenderDrawData& draw_data,
        RenderingResourceManager& resource_manager,
        IGraphicsBackend& backend)
    {
        const auto fail = [&backend](const char* message) -> std::vector<RenderPass>
        {
            TBX_TRACE_ERROR_ONCE("Rendering pipeline pass creation failed: {}", message);
            backend.end_frame();
            return {};
        };
        const auto fail_result =
            [&backend](const char* operation, const Result& result) -> std::vector<RenderPass>
        {
            TBX_TRACE_ERROR_ONCE(
                "Rendering pipeline pass creation failed during {}: {}",
                operation,
                result.get_report());
            backend.end_frame();
            return {};
        };

        // Upload data
        const auto light_shader_data = internal::make_light_shader_data(
            draw_data,
            draw_data.lighting.ambient_color_sum,
            draw_data.lighting.ambient_intensity_sum,
            draw_data.lighting.directional_light_count);

        const auto frame_shader_data = FrameShaderData {
            .time = frame.time,
            .delta_time = frame.delta_time,
            .viewport_size = Vec2(
                static_cast<float>(frame.viewport.dimensions.width),
                static_cast<float>(frame.viewport.dimensions.height)),
        };
        const auto camera_shader_data = CameraShaderData {
            .view = frame.camera.view,
            .projection = frame.camera.projection,
            .view_projection = frame.camera.view_projection,
            .inverse_view = frame.camera.inverse_view,
            .inverse_projection = frame.camera.inverse_projection,
            .world_position = Vec4(frame.camera.position, 1.0F),
        };

        const auto frame_uniform = resource_manager.upload_uniform_buffer(
            BINDING_FRAME_DATA,
            "Frame Shader Data",
            "Toybox/Uniforms/Frame",
            frame_index,
            &frame_shader_data,
            static_cast<uint64>(sizeof(frame_shader_data)));
        const auto camera_uniform = resource_manager.upload_uniform_buffer(
            BINDING_CAMERA_DATA,
            "Camera Shader Data",
            "Toybox/Uniforms/Camera",
            frame_index,
            &camera_shader_data,
            static_cast<uint64>(sizeof(camera_shader_data)));
        const auto light_uniform = resource_manager.upload_uniform_buffer(
            BINDING_LIGHT_DATA,
            "Light Shader Data",
            "Toybox/Uniforms/Light",
            frame_index,
            &light_shader_data,
            static_cast<uint64>(sizeof(light_shader_data)));
        if (!frame_uniform.resource.is_valid() || !camera_uniform.resource.is_valid()
            || !light_uniform.resource.is_valid())
        {
            return fail("frame, camera, or light uniform upload returned an invalid resource.");
        }

        // Sky Pass: Draws sky geometry into the final color target before scene lighting.
        auto sky_pass = RenderPass {
            .desc =
                GraphicsPassDesc {
                    .color_targets = {gbuffer.final_color.resource},
                    .clear_color = Color::BLACK,
                    .clear_flags = GraphicsClearFlags::COLOR,
                    .debug_name = "Toybox Skybox Pass",
                },
        };
        if (draw_data.sky.material.get_handle().is_valid())
        {
            const auto sky_mesh_handle = draw_data.sky.type == SkyType::BOX
                                             ? Handle("Toybox/SkyBox")
                                             : Handle("Toybox/SkySphere");
            const Mesh& sky_mesh = draw_data.sky.type == SkyType::BOX ? Mesh::CUBE : Mesh::SPHERE;
            auto uploaded_sky_mesh = RenderingMeshUploadData();
            auto result = resource_manager.upload_static_runtime_mesh(
                sky_mesh_handle,
                sky_mesh,
                uploaded_sky_mesh);
            if (!result)
                return fail_result("sky mesh upload", result);

            auto sky_material_upload = RenderingMaterialUploadData();
            result = resource_manager.upload_material(draw_data.sky.material, sky_material_upload);
            if (!result)
                return fail_result("sky material upload", result);
            append_fallback_texture(
                resource_manager,
                PARAM_SKYBOX_TEXTURE,
                sky_material_upload.textures);
            append_fallback_texture(
                resource_manager,
                PARAM_SECONDARY_SKYBOX_TEXTURE,
                sky_material_upload.textures);

            const auto sky_instance = internal::RenderMeshInstance {};
            const auto sky_instance_buffer = resource_manager.upload_instance_buffer(
                "Toybox/Instances/Sky",
                frame_index,
                &sky_instance,
                static_cast<uint64>(sizeof(sky_instance)));
            const auto sky_material_uniform = resource_manager.upload_uniform_buffer(
                BINDING_MATERIAL_DATA,
                "Material Shader Data",
                "Toybox/Uniforms/Material/Sky",
                frame_index,
                sky_material_upload.uniform_values.data(),
                static_cast<uint64>(sky_material_upload.uniform_values.size() * sizeof(Vec4)));
            if (!sky_instance_buffer.resource.is_valid()
                || !sky_material_uniform.resource.is_valid())
            {
                return fail(
                    "sky instance or material uniform upload returned an invalid resource.");
            }

            sky_pass.indexed_draws.push_back(
                GraphicsIndexedDrawCommand {
                    .pipeline = sky_material_upload.pipeline,
                    .index_buffer = uploaded_sky_mesh.index_buffer,
                    .index_type = GraphicsIndexType::UINT32,
                    .vertex_buffers =
                        {
                            GraphicsResourceBinding {
                                .slot = VERTEX_BUFFER_SLOT_MESH,
                                .resource = uploaded_sky_mesh.vertex_buffer,
                            },
                            sky_instance_buffer,
                        },
                    .uniform_buffers =
                        {
                            frame_uniform,
                            camera_uniform,
                            light_uniform,
                            sky_material_uniform,
                        },
                    .textures = sky_material_upload.textures,
                    .draw =
                        GraphicsDrawIndexedDesc {
                            .index_type = GraphicsIndexType::UINT32,
                            .index_count = uploaded_sky_mesh.index_count,
                        },
                });
        }

        // Opaque Pass: Draws opaque scene geometry into deferred shading targets and depth.
        auto opaque_pass = RenderPass {
            .desc =
                GraphicsPassDesc {
                    .color_targets =
                        {
                            gbuffer.albedo.resource,
                            gbuffer.normal.resource,
                            gbuffer.material.resource,
                            gbuffer.emissive.resource,
                        },
                    .depth_stencil_target = gbuffer.depth.resource,
                    .clear_color = Color::BLACK,
                    .clear_flags = GraphicsClearFlags::COLOR_DEPTH,
                    .debug_name = "Toybox Opaque Pass",
                },
        };
        auto result = internal::append_batch_draws(
            resource_manager,
            frame_index,
            frame_uniform,
            camera_uniform,
            light_uniform,
            draw_data.opaque_batches,
            opaque_pass);
        if (!result)
            return fail_result("opaque batch draw upload", result);

        // Alpha Cutout Pass: Reserved for alpha-tested geometry that should write deferred targets
        // with depth.
        auto alpha_cutout_pass = RenderPass {
            .desc =
                GraphicsPassDesc {
                    .clear_flags = GraphicsClearFlags::NONE,
                    .debug_name = "Toybox Alpha Cutout Scene Pass",
                },
        };

        // Transparent Pass: Draws alpha-blended geometry forward over the lit scene color.
        auto transparent_pass = RenderPass {
            .desc =
                GraphicsPassDesc {
                    .color_targets = {gbuffer.final_color.resource},
                    .depth_stencil_target = gbuffer.depth.resource,
                    .clear_flags = GraphicsClearFlags::NONE,
                    .debug_name = "Toybox Transparent Forward Pass",
                },
        };
        result = internal::append_batch_draws(
            resource_manager,
            frame_index,
            frame_uniform,
            camera_uniform,
            light_uniform,
            draw_data.transparent_batches,
            transparent_pass);
        if (!result)
            return fail_result("transparent batch draw upload", result);

        // Lighting Pass: Computes deferred lighting from the GBuffer into the final color target.
        auto lighting_pass = RenderPass {
            .desc =
                GraphicsPassDesc {
                    .color_targets = {gbuffer.final_color.resource},
                    .clear_color = Color::BLACK,
                    .clear_flags = GraphicsClearFlags::NONE,
                    .debug_name = "Toybox Deferred Lighting Pass",
                },
        };
        auto lighting_material = RenderingMaterialUploadData();
        result = resource_manager.upload_material(
            MaterialInstance(tbx::DeferredLightingMaterial::HANDLE),
            lighting_material);
        if (!result)
            return fail_result("deferred lighting material upload", result);
        lighting_pass.draws.push_back(
            GraphicsDrawCommand {
                .pipeline = lighting_material.pipeline,
                .uniform_buffers = {frame_uniform, camera_uniform, light_uniform},
                .textures =
                    {
                        gbuffer.albedo,
                        gbuffer.normal,
                        gbuffer.material,
                        gbuffer.emissive,
                        gbuffer.depth,
                    },
                .vertex_count = 3U,
            });

        // Post Process Pass: Applies fullscreen post processing from final color to the current
        // frame target.
        auto post_process_pass = RenderPass {
            .desc =
                GraphicsPassDesc {
                    .clear_color = Color::BLACK,
                    .clear_flags = GraphicsClearFlags::COLOR,
                    .debug_name = "Toybox Post Process Pass",
                },
        };
        auto post_material = RenderingMaterialUploadData();
        result = resource_manager.upload_material(
            MaterialInstance(tbx::TonemapPostMaterial::HANDLE),
            post_material);
        if (!result)
            return fail_result("post process material upload", result);
        post_process_pass.draws.push_back(
            GraphicsDrawCommand {
                .pipeline = post_material.pipeline,
                .uniform_buffers = {frame_uniform, camera_uniform, light_uniform},
                .textures = {gbuffer.final_color},
                .vertex_count = 3U,
            });

        // Create pass list and return
        auto passes = std::vector<RenderPass>(6);
        passes.push_back(std::move(sky_pass));
        passes.push_back(std::move(opaque_pass));
        passes.push_back(std::move(alpha_cutout_pass));
        passes.push_back(std::move(transparent_pass));
        passes.push_back(std::move(lighting_pass));
        passes.push_back(std::move(post_process_pass));
        return passes;
    }

    static RenderDrawData create_draw_data(
        const EntityRegistry& entity_registry,
        const Vec3& camera_position,
        AssetManager& asset_manager,
        const float local_light_max_distance)
    {
        auto draw_data = RenderDrawData();

        for (auto& entity : entity_registry.get_with<Sky>())
        {
            draw_data.sky = entity.get_component<Sky>();
            if (!draw_data.sky.material.get_handle().is_valid())
                draw_data.sky.material = MaterialInstance(TexturedSkyMaterial::HANDLE);

            const Color clear_color =
                draw_data.sky.material.get_parameter_or(TexturedSkyMaterial::COLOR, Color::BLACK);
            draw_data.clear_color = clear_color;

            // TODO: Warn if there is more than one sky, and say picking first found as only one is
            // supported.
            break;
        }

        for (auto& entity : entity_registry.get_with<DirectionalLight>())
        {
            const auto& light = entity.get_component<DirectionalLight>();
            const Transform transform = get_optional_transform(entity);
            const Vec3 direction = normalize_or_zero(transform.rotation * Vec3(0.0F, 0.0F, -1.0F));
            const int32 shadow_index = reserve_shadow_index(
                draw_data.lighting.shadow_layer_count,
                light.cast_shadows,
                DIRECTIONAL_SHADOW_CASCADE_COUNT);

            draw_data.lighting.ambient_color_sum +=
                Vec3(light.color.r, light.color.g, light.color.b);
            draw_data.lighting.ambient_intensity_sum += light.ambient;
            ++draw_data.lighting.directional_light_count;

            append_light(
                draw_data,
                RenderLight {
                    .type = static_cast<uint>(SHADER_LIGHT_TYPE_DIRECTIONAL),
                    .color = light.color,
                    .intensity = light.intensity,
                    .shadow_index = shadow_index,
                    .shadow_layer_count = shadow_index >= 0 ? DIRECTIONAL_SHADOW_CASCADE_COUNT : 0U,
                    .direction = direction,
                });
        }

        for (auto& entity : entity_registry.get_with<PointLight>())
        {
            const auto& light = entity.get_component<PointLight>();
            const Transform transform = get_optional_transform(entity);
            if (!is_within_local_light_range(
                    transform.position,
                    camera_position,
                    local_light_max_distance))
                continue;

            const int32 shadow_index =
                reserve_shadow_index(draw_data.lighting.shadow_layer_count, light.cast_shadows, 1U);
            append_light(
                draw_data,
                RenderLight {
                    .type = static_cast<uint>(SHADER_LIGHT_TYPE_POINT),
                    .color = light.color,
                    .intensity = light.intensity,
                    .shadow_index = shadow_index,
                    .shadow_layer_count = shadow_index >= 0 ? 1U : 0U,
                    .position = transform.position,
                    .range = light.range,
                });
        }

        for (auto& entity : entity_registry.get_with<SpotLight>())
        {
            const auto& light = entity.get_component<SpotLight>();
            const Transform transform = get_optional_transform(entity);
            if (!is_within_local_light_range(
                    transform.position,
                    camera_position,
                    local_light_max_distance))
                continue;

            const Vec3 direction = normalize_or_zero(transform.rotation * Vec3(0.0F, 0.0F, -1.0F));
            const int32 shadow_index =
                reserve_shadow_index(draw_data.lighting.shadow_layer_count, light.cast_shadows, 1U);
            append_light(
                draw_data,
                RenderLight {
                    .type =
                        static_cast<uint>(SHADER_LIGHT_TYPE_SPOT), // TODO: Convert types to ints
                    .color = light.color,
                    .intensity = light.intensity,
                    .inner_cone = angle_to_cosine(light.inner_angle),
                    .outer_cone = angle_to_cosine(light.outer_angle),
                    .shadow_index = shadow_index,
                    .shadow_layer_count = shadow_index >= 0 ? 1U : 0U,
                    .position = transform.position,
                    .direction = direction,
                    .range = light.range,
                });
        }

        for (auto& entity : entity_registry.get_with<AreaLight>())
        {
            const auto& light = entity.get_component<AreaLight>();
            const Transform transform = get_optional_transform(entity);
            if (!is_within_local_light_range(
                    transform.position,
                    camera_position,
                    local_light_max_distance))
                continue;

            const int32 shadow_index =
                reserve_shadow_index(draw_data.lighting.shadow_layer_count, light.cast_shadows, 1U);
            append_light(
                draw_data,
                RenderLight {
                    .type = static_cast<uint>(SHADER_LIGHT_TYPE_POINT),
                    .color = light.color,
                    .intensity = light.intensity,
                    .shadow_index = shadow_index,
                    .shadow_layer_count = shadow_index >= 0 ? 1U : 0U,
                    .position = transform.position,
                    .range = light.range,
                });
        }

        draw_data.shadows.count = draw_data.lighting.shadow_layer_count;

        for (auto& entity : entity_registry.get_with<StaticMesh>())
        {
            const auto& static_mesh = entity.get_component<StaticMesh>();
            const MaterialInstance material = entity.has_component<MaterialInstance>()
                                                  ? entity.get_component<MaterialInstance>()
                                                  : MaterialInstance();
            const MaterialConfig config = resolve_draw_material_config(asset_manager, material);
            const uint64 material_key = hash(material);
            const Transform transform = get_optional_transform(entity);
            const Mat4 model_matrix = build_transform_matrix(transform);
            const auto mesh = RenderMesh {
                .handle = static_mesh.handle,
                .data = static_mesh,
            };
            auto& batches = config.blend_mode == MaterialBlendMode::ALPHA_BLEND
                                ? draw_data.transparent_batches
                                : draw_data.opaque_batches;
            append_batch(
                batches,
                make_batch_hash(static_mesh.handle.get_id(), nullptr, material_key),
                mesh,
                material,
                RenderMeshInstance {
                    .model_matrix = model_matrix,
                    .normal_matrix = normal(model_matrix),
                });
        }

        for (auto& entity : entity_registry.get_with<DynamicMesh>())
        {
            const auto& dynamic_mesh = entity.get_component<DynamicMesh>();
            const auto mesh_data = dynamic_mesh.get_data();
            const MaterialInstance material = entity.has_component<MaterialInstance>()
                                                  ? entity.get_component<MaterialInstance>()
                                                  : MaterialInstance();
            const MaterialConfig config = resolve_draw_material_config(asset_manager, material);
            const uint64 material_key = hash(material);
            const Transform transform = get_optional_transform(entity);
            const Mat4 model_matrix = build_transform_matrix(transform);
            const auto mesh = RenderMesh {
                .handle = Handle(
                    "Toybox/DynamicMesh_"
                    + std::to_string(std::hash<DynamicMeshData*> {}(mesh_data.get()))),
                .data = dynamic_mesh,
            };
            auto& batches = config.blend_mode == MaterialBlendMode::ALPHA_BLEND
                                ? draw_data.transparent_batches
                                : draw_data.opaque_batches;
            append_batch(
                batches,
                make_batch_hash(
                    Uuid(static_cast<uint32>(reinterpret_cast<std::uintptr_t>(mesh_data.get()))),
                    mesh_data.get(),
                    material_key),
                mesh,
                material,
                RenderMeshInstance {
                    .model_matrix = model_matrix,
                    .normal_matrix = normal(model_matrix),
                });
        }

        return draw_data;
    }

    static Result execute_draw_command(
        IGraphicsBackend& backend,
        const GraphicsDrawCommand& command)
    {
        auto result = backend.bind_pipeline(command.pipeline);
        if (!result)
            return result;

        for (const auto& binding : command.vertex_buffers)
        {
            result = backend.bind_vertex_buffer(binding.slot, binding.resource);
            if (!result)
                return result;
        }

        for (const auto& binding : command.uniform_buffers)
        {
            result = backend.bind_uniform_buffer(binding.slot, binding.resource);
            if (!result)
                return result;
        }

        for (const auto& binding : command.storage_buffers)
        {
            result = backend.bind_storage_buffer(binding.slot, binding.resource);
            if (!result)
                return result;
        }

        for (const auto& binding : command.textures)
        {
            result = backend.bind_texture(binding.slot, binding.resource);
            if (!result)
                return result;
        }

        for (const auto& binding : command.samplers)
        {
            result = backend.bind_sampler(binding.slot, binding.resource);
            if (!result)
                return result;
        }

        return backend.draw(command.vertex_count, command.vertex_offset);
    }

    static Result execute_draw_command(
        IGraphicsBackend& backend,
        const GraphicsIndexedDrawCommand& command)
    {
        auto result = backend.bind_pipeline(command.pipeline);
        if (!result)
            return result;

        for (const auto& binding : command.vertex_buffers)
        {
            result = backend.bind_vertex_buffer(binding.slot, binding.resource);
            if (!result)
                return result;
        }

        result = backend.bind_index_buffer(command.index_buffer, command.index_type);
        if (!result)
            return result;

        for (const auto& binding : command.uniform_buffers)
        {
            result = backend.bind_uniform_buffer(binding.slot, binding.resource);
            if (!result)
                return result;
        }

        for (const auto& binding : command.storage_buffers)
        {
            result = backend.bind_storage_buffer(binding.slot, binding.resource);
            if (!result)
                return result;
        }

        for (const auto& binding : command.textures)
        {
            result = backend.bind_texture(binding.slot, binding.resource);
            if (!result)
                return result;
        }

        for (const auto& binding : command.samplers)
        {
            result = backend.bind_sampler(binding.slot, binding.resource);
            if (!result)
                return result;
        }

        return backend.draw_indexed(command.draw);
    }

    static Result execute_passes(
        IGraphicsBackend& backend,
        const std::vector<RenderPass>& render_passes)
    {
        for (const auto& render_pass : render_passes)
        {
            auto result = backend.begin_pass(render_pass.desc);
            if (!result)
                return result;

            for (const auto& draw_command : render_pass.draws)
            {
                result = execute_draw_command(backend, draw_command);
                if (!result)
                {
                    (void)backend.end_pass();
                    return result;
                }
            }

            for (const auto& indexed_draw_command : render_pass.indexed_draws)
            {
                result = execute_draw_command(backend, indexed_draw_command);
                if (!result)
                {
                    (void)backend.end_pass();
                    return result;
                }
            }

            result = backend.end_pass();
            if (!result)
                return result;
        }

        return {};
    }

    static FrameData create_frame_data(
        const uint frame,
        const DeltaTime delta_time,
        const float elapsed_time,
        const Size resolution,
        const EntityRegistry& entity_registry,
        const IWindowManager& window_manager)
    {
        auto render_camera = Camera();
        auto cam_transform = Transform();
        for (auto& entity : entity_registry.get_with<Camera>())
        {
            render_camera = entity.get_component<Camera>();
            // Cameras without a Transform render from identity so authoring transform-less test
            // scenes still produces deterministic frame data.
            if (entity.has_component<Transform>())
                cam_transform = get_world_space_transform(entity);
            break;
        }

        auto render_target = window_manager.get_main_window();
        auto cam_target = render_camera.get_render_target();
        if (cam_target.is_valid())
            render_target = cam_target;

        auto target_resolution = window_manager.get_size(render_target);
        if (target_resolution.width == 0U || target_resolution.height == 0U)
            target_resolution = Size {1U, 1U};

        auto render_resolution = resolution;
        if (render_resolution.width == 0U || render_resolution.height == 0U)
            render_resolution = target_resolution;

        auto render_viewport = Viewport {Vec2(0.0F), render_resolution};
        auto cam_viewport = render_camera.get_viewport();
        if (!cam_viewport.is_zero())
            render_viewport = cam_viewport;

        render_camera.set_aspect(render_resolution.get_aspect_ratio());
        const Mat4 cam_view_matrix =
            render_camera.get_view_matrix(cam_transform.position, cam_transform.rotation);
        const Mat4 cam_projection_matrix = render_camera.get_projection_matrix();
        const Mat4 cam_view_projection_matrix = cam_projection_matrix * cam_view_matrix;

        return FrameData {
            .index = frame,
            .time = elapsed_time,
            .delta_time = static_cast<float>(delta_time.seconds),
            .resolution = render_resolution,
            .viewport = render_viewport,
            .target = render_target,
            .camera =
                RenderCamera {
                    .view = cam_view_matrix,
                    .position = cam_transform.position,
                    .projection = cam_projection_matrix,
                    .view_projection = cam_view_projection_matrix,
                    .inverse_view = inverse(cam_view_matrix),
                    .inverse_projection = inverse(cam_projection_matrix),
                },
        };
    }

    static GraphicsTextureDesc make_deferred_color_target_desc(
        const Size& render_resolution,
        const GraphicsTextureFormat format,
        const std::string& debug_name)
    {
        return GraphicsTextureDesc {
            .usage = GraphicsTextureUsage::SAMPLED_RENDER_TARGET,
            .format = format,
            .size = render_resolution,
            .mip_count = 1U,
            .array_layer_count = 1U,
            .debug_name = debug_name,
        };
    }

    static GraphicsTextureDesc make_deferred_depth_target_desc(
        const Size& render_resolution,
        const std::string& debug_name)
    {
        return GraphicsTextureDesc {
            .usage = GraphicsTextureUsage::SAMPLED_DEPTH_STENCIL,
            .format = GraphicsTextureFormat::DEPTH32_FLOAT,
            .size = render_resolution,
            .mip_count = 1U,
            .array_layer_count = 1U,
            .debug_name = debug_name,
        };
    }

    static Result create_gbuffer(
        RenderingResourceManager& resource_manager,
        const FrameData& frame,
        GBuffer& out_buff)
    {
        const Size viewport_size = frame.viewport.dimensions;
        const std::string render_target_key =
            std::to_string(viewport_size.width) + "x" + std::to_string(viewport_size.height);

        out_buff.albedo = resource_manager.upload_texture(
            BINDING_GBUFFER_ALBEDO,
            "Toybox/GBuffer/Albedo/" + render_target_key,
            make_deferred_color_target_desc(
                viewport_size,
                GraphicsTextureFormat::RGBA8,
                "Toybox GBuffer Albedo"));
        out_buff.normal = resource_manager.upload_texture(
            BINDING_GBUFFER_NORMAL,
            "Toybox/GBuffer/Normal/" + render_target_key,
            make_deferred_color_target_desc(
                viewport_size,
                GraphicsTextureFormat::RGBA16_FLOAT,
                "Toybox GBuffer Normal"));
        out_buff.material = resource_manager.upload_texture(
            BINDING_GBUFFER_MATERIAL,
            "Toybox/GBuffer/Material/" + render_target_key,
            make_deferred_color_target_desc(
                viewport_size,
                GraphicsTextureFormat::RGBA8,
                "Toybox GBuffer Material"));
        out_buff.emissive = resource_manager.upload_texture(
            BINDING_GBUFFER_EMISSIVE,
            "Toybox/GBuffer/Emissive/" + render_target_key,
            make_deferred_color_target_desc(
                viewport_size,
                GraphicsTextureFormat::RGBA16_FLOAT,
                "Toybox GBuffer Emissive"));
        out_buff.depth = resource_manager.upload_texture(
            BINDING_GBUFFER_DEPTH,
            "Toybox/GBuffer/Depth/" + render_target_key,
            make_deferred_depth_target_desc(viewport_size, "Toybox GBuffer Depth"));
        out_buff.final_color = resource_manager.upload_texture(
            BINDING_POST_SOURCE_COLOR,
            "Toybox/FinalColor/" + render_target_key,
            make_deferred_color_target_desc(
                viewport_size,
                GraphicsTextureFormat::RGBA16_FLOAT,
                "Toybox Final Color"));

        if (!out_buff.albedo.resource.is_valid() || !out_buff.normal.resource.is_valid()
            || !out_buff.material.resource.is_valid() || !out_buff.emissive.resource.is_valid()
            || !out_buff.depth.resource.is_valid() || !out_buff.final_color.resource.is_valid())
        {
            return Result(false, "Frame pipeline failed: deferred target upload failed.");
        }

        return {};
    }
}
