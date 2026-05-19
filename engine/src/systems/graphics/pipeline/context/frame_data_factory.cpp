#include "tbx/systems/graphics/pipeline/context/frame_data_factory.h"
#include "tbx/systems/assets/fallbacks.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/types/components/material_instance.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/material.h"
#include "tbx/types/vertex.h"
#include <string>
#include <utility>

namespace tbx
{
    FrameDataFactory::FrameDataFactory(
        std::weak_ptr<EntityRegistry> entity_registry,
        std::weak_ptr<IWindowManager> window_manager,
        Window default_output_window,
        const GraphicsSettings& settings)
        : _entity_registry(std::move(entity_registry))
        , _window_manager(std::move(window_manager))
        , _default_output_window(std::move(default_output_window))
        , _configured_resolution(settings.resolution.value)
    {
    }

    Result FrameDataFactory::create(
        GraphicsResourceManager& resource_manager,
        const uint64 frame_index,
        FrameData& out_frame_data) const
    {
        out_frame_data = {};
        out_frame_data.frame_index = frame_index;

        const auto entity_registry = _entity_registry.lock();
        const auto window_manager = _window_manager.lock();
        if (!entity_registry || !window_manager)
            return Result(false, "Frame data factory: required scene/window services unavailable.");

        const Window output_window = _default_output_window.is_valid()
                                       ? _default_output_window
                                       : Window("Toybox/MainWindow");
        Size output_resolution = window_manager->get_size(output_window);
        if (output_resolution.width == 0U || output_resolution.height == 0U)
            output_resolution = Size {1U, 1U};

        Size render_resolution = _configured_resolution;
        if (render_resolution.width == 0U || render_resolution.height == 0U)
            render_resolution = output_resolution;

        out_frame_data.output_window = output_window;
        out_frame_data.output_resolution = output_resolution;
        out_frame_data.render_resolution = render_resolution;
        out_frame_data.view = GraphicsView {
            .camera = Camera {},
            .viewport = Viewport {.position = Vec2(0.0F), .dimensions = render_resolution},
        };

        auto geometry_pass = GraphicsRenderPass {
            .pass = GraphicsPassDesc {
                .clear_color = Color::BLACK,
                .clear_depth = 1.0F,
                .clear_flags = GraphicsClearFlags::COLOR_DEPTH,
                .debug_name = "Toybox Opaque Scene Pass",
            },
            .viewport = out_frame_data.view.viewport,
        };

        auto transparent_pass = GraphicsRenderPass {
            .pass = GraphicsPassDesc {
                .clear_depth = 1.0F,
                .clear_flags = GraphicsClearFlags::NONE,
                .debug_name = "Toybox Transparent Forward Pass",
            },
            .viewport = out_frame_data.view.viewport,
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
            max_dynamic_index_count = std::max(max_dynamic_index_count, static_cast<uint32>(mesh.indices.size()));
            if (mesh.vertices.empty() || mesh.indices.empty())
                continue;

            auto model_resource = GraphicsModelResource {};
            const auto model_handle = Handle(
                std::string("Toybox/DynamicMesh/") + to_string(entity.get_id()));
            if (const auto result = resource_manager.upload(
                    model_handle,
                    mesh,
                    model_resource);
                !result)
            {
                return result;
            }

            auto material_resource = GraphicsMaterialDrawResource {};
            if (const auto result = resource_manager.upload(fallback_material, material_resource);
                !result)
            {
                return result;
            }

            for (const auto& mesh_resource : model_resource.meshes)
            {
                if (!mesh_resource.vertex_buffer.is_valid() || !mesh_resource.index_buffer.is_valid()
                    || mesh_resource.index_count == 0U)
                {
                    continue;
                }

                geometry_pass.indexed_draws.push_back(
                    GraphicsIndexedDrawCommand {
                        .pipeline = material_resource.pipeline,
                        .index_buffer = mesh_resource.index_buffer,
                        .index_type = GraphicsIndexType::UINT32,
                        .vertex_buffers = {GraphicsResourceBinding {
                            .slot = 0U,
                            .resource = mesh_resource.vertex_buffer,
                        }},
                        .draw = GraphicsDrawIndexedDesc {
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
                return result;
            }

            auto material_resource = GraphicsMaterialDrawResource {};
            if (const auto result = resource_manager.upload(fallback_material, material_resource);
                !result)
            {
                return result;
            }

            for (const auto& mesh_resource : model_resource.meshes)
            {
                if (!mesh_resource.vertex_buffer.is_valid() || !mesh_resource.index_buffer.is_valid()
                    || mesh_resource.index_count == 0U)
                {
                    continue;
                }

                geometry_pass.indexed_draws.push_back(
                    GraphicsIndexedDrawCommand {
                        .pipeline = material_resource.pipeline,
                        .index_buffer = mesh_resource.index_buffer,
                        .index_type = GraphicsIndexType::UINT32,
                        .vertex_buffers = {GraphicsResourceBinding {
                            .slot = 0U,
                            .resource = mesh_resource.vertex_buffer,
                        }},
                        .draw = GraphicsDrawIndexedDesc {
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

        for (auto& entity : entity_registry->get_with<Sky>())
        {
            has_sky_geometry = true;
            auto& sky = entity.get_component<Sky>();
            auto material_resource = GraphicsMaterialDrawResource {};
            if (const auto result = resource_manager.upload(sky.material, material_resource);
                !result)
            {
                return result;
            }

            auto model_resource = GraphicsModelResource {};
            if (const auto result = resource_manager.upload(
                    Handle("Toybox/SkyDome"),
                    sky_dome,
                    model_resource);
                !result)
            {
                return result;
            }

            auto sky_pass = GraphicsRenderPass {
                .pass = GraphicsPassDesc {
                    .clear_color = Color::BLACK,
                    .clear_depth = 1.0F,
                    .clear_flags = GraphicsClearFlags::COLOR_DEPTH,
                    .debug_name = "Toybox Skybox Pass",
                },
                .viewport = out_frame_data.view.viewport,
            };

            for (const auto& mesh_resource : model_resource.meshes)
            {
                sky_pass.indexed_draws.push_back(
                    GraphicsIndexedDrawCommand {
                        .pipeline = material_resource.pipeline,
                        .index_buffer = mesh_resource.index_buffer,
                        .index_type = GraphicsIndexType::UINT32,
                        .vertex_buffers = {GraphicsResourceBinding {
                            .slot = 0U,
                            .resource = mesh_resource.vertex_buffer,
                        }},
                        .draw = GraphicsDrawIndexedDesc {
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

            out_frame_data.skybox_passes.push_back(std::move(sky_pass));
        }

        // Geometry remains deferred: opaque and alpha-cutout populate the scene pass first. The
        // lighting pass is a fullscreen deferred resolve. Transparent materials are reserved for a
        // separate forward pass because blending requires final scene color ordering.
        if (!geometry_pass.indexed_draws.empty())
        {
            if (!out_frame_data.skybox_passes.empty())
                geometry_pass.pass.clear_flags = GraphicsClearFlags::DEPTH;
            out_frame_data.opaque_passes.push_back(std::move(geometry_pass));
        }

        const bool should_add_lighting_pass = has_static_geometry || has_sky_geometry || max_dynamic_index_count > 3U;
        if (!should_add_lighting_pass)
        {
            if (!transparent_pass.indexed_draws.empty())
                out_frame_data.transparent_passes.push_back(std::move(transparent_pass));

            return {};
        }

        auto light_model_resource = GraphicsModelResource {};
        if (const auto result = resource_manager.upload(
                Handle("Toybox/DeferredLightingQuad"),
                fullscreen_quad,
                light_model_resource);
            !result)
        {
            return result;
        }

        const auto fallback_shader = make_fallback_shader();
        if (!fallback_shader)
            return Result(false, "Frame data factory: failed to create lighting shader.");

        auto light_pipeline = Uuid {};
        if (const auto result = resource_manager.upload(
                GraphicsPipelineDesc {
                    .shader = *fallback_shader,
                    .primitive_type = GraphicsPrimitiveType::TRIANGLES,
                    .is_depth_test_enabled = false,
                    .is_depth_write_enabled = false,
                    .is_blending_enabled = false,
                    .is_culling_enabled = false,
                    .debug_name = "Toybox Deferred Lighting Pipeline",
                },
                light_pipeline);
            !result)
        {
            return result;
        }

        auto lighting_pass = GraphicsRenderPass {
            .pass = GraphicsPassDesc {
                .clear_color = Color::BLACK,
                .clear_depth = 1.0F,
                .clear_flags = GraphicsClearFlags::COLOR_DEPTH,
                .debug_name = "Toybox Lighting Pass",
            },
            .viewport = out_frame_data.view.viewport,
        };
        lighting_pass.draws.push_back(
            GraphicsDrawCommand {
                .pipeline = light_pipeline,
                .vertex_count = 3U,
                .vertex_offset = 0U,
            });
        out_frame_data.lighting_passes.push_back(std::move(lighting_pass));

        if (!transparent_pass.indexed_draws.empty())
            out_frame_data.transparent_passes.push_back(std::move(transparent_pass));

        return {};
    }
}
