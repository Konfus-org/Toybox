#include "tbx/systems/graphics/rendering_pipeline.h"
#include "systems/graphics/internal/render_metrics_internal.h"
#include "systems/graphics/internal/rendering_pipeline_internal.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/graphics/settings.h"
#include "tbx/systems/graphics/shader_bindings.h"
#include "tbx/types/components/camera.h"
#include "tbx/types/components/light.h"
#include "tbx/types/components/material_instance.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/components/sky.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/matrices.h"
#include "tbx/types/render_target.h"
#include "tbx/types/shader.h"
#include "tbx/types/trig.h"
#include "tbx/types/viewport.h"
#include "tbx/utils/hash.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace tbx
{
    RenderingPipeline::RenderingPipeline(
        std::weak_ptr<IGraphicsBackend> backend,
        std::weak_ptr<EntityRegistry> entity_registry,
        std::weak_ptr<AssetManager> asset_manager,
        std::weak_ptr<IWindowManager> window_manager,
        const GraphicsSettings& settings)
        : _entity_registry(std::move(entity_registry))
        , _window_manager(std::move(window_manager))
        , _resource_uploader(std::move(backend), std::move(asset_manager))
    {
    }

    Result RenderingPipeline::execute(
        IGraphicsBackend& backend,
        const GraphicsSettings& settings,
        const DeltaTime& delta_time,
        const uint frame_index)
    {
        _elapsed_time += static_cast<float>(delta_time.seconds);

        const auto entity_registry = _entity_registry.lock();
        if (!entity_registry)
            return Result(false, "Rendering pipeline setup failed: scene service unavailable.");

        const auto window_manager = _window_manager.lock();
        if (!window_manager)
            return Result(false, "Rendering pipeline setup failed: window manager unavailable.");

        // 1.) Construct frame and begin..
        auto frame = internal::create_frame_data(
            frame_index,
            delta_time,
            _elapsed_time,
            settings.resolution,
            *entity_registry,
            *window_manager);
        auto result = backend.begin_frame(frame.target);
        if (!result)
            return result;

        // 2.) Setup viewport to render into
        result = backend.set_viewport(frame.viewport);
        if (!result)
        {
            backend.end_frame();
            return result;
        }

        // 3.) Create gbuffer...
        auto gbuffer = internal::GBuffer();
        result = internal::create_gbuffer(_resource_uploader, _resource_tracker, frame, gbuffer);
        if (!result)
        {
            backend.end_frame();
            return result;
        }

        // 4.) Define passes...
        const auto passes = std::vector<RenderPass> {
            RenderPass {
                .desc =
                    GraphicsPassDesc {
                        .color_targets = {gbuffer.final_color.resource},
                        .clear_color = Color::BLACK,
                        .clear_flags = GraphicsClearFlags::COLOR,
                        .debug_name = "Toybox Skybox Pass",
                    },
            },
            RenderPass {
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
                        .debug_name = "Toybox GBuffer Pass", // Do we need an explicit gbuffer pass?
                                                             // Shouldn't this be a Opaque pass?
                    },
            },
            RenderPass {
                .desc =
                    GraphicsPassDesc {
                        .clear_flags = GraphicsClearFlags::NONE,
                        .debug_name = "Toybox Alpha Cutout Scene Pass",
                    },
            },
            RenderPass {
                .desc =
                    GraphicsPassDesc {
                        .color_targets = {gbuffer.final_color.resource},
                        .depth_stencil_target = gbuffer.depth.resource,
                        .clear_flags = GraphicsClearFlags::NONE,
                        .debug_name = "Toybox Transparent Forward Pass",
                    },
            },
            RenderPass {
                .desc =
                    GraphicsPassDesc {
                        .color_targets = {gbuffer.final_color.resource},
                        .clear_color = Color::BLACK,
                        .clear_flags = GraphicsClearFlags::NONE,
                        .debug_name = "Toybox Deferred Lighting Pass",
                    },
            },
            RenderPass {
                .desc =
                    GraphicsPassDesc {
                        .clear_color = Color::BLACK,
                        .clear_flags = GraphicsClearFlags::COLOR,
                        .debug_name = "Toybox Post Process Pass",
                    },
            },
        };

        // 5.) Go over registry and extract render data
        auto draw_data = RenderDrawData();
        {
            // TODO: Define clear color as the color of the sky
            const auto& sky = entity_registry->first_with<Sky>().get_component<Sky>();

            // Get light/shadow data
            entity_registry->for_each_with<DirectionalLight>(
                [&passes](const Entity& ent)
                {
                });

            entity_registry->for_each_with<PointLight>(
                [&passes](const Entity& ent)
                {
                });

            entity_registry->for_each_with<SpotLight>(
                [&passes](const Entity& ent)
                {
                });

            // entity_registry->for_each_with<AreaLight>(
            //    [&passes](const Entity& ent)
            //    {
            //    });

            // Batch things by mesh/material and extract data for rendering
            entity_registry->for_each_with<StaticMesh>(
                [&passes](const Entity& ent)
                {
                });

            entity_registry->for_each_with<DynamicMesh>(
                [&passes](const Entity& ent)
                {
                });
        }

        auto frame_uniforms = internal::FrameUniformBindings();
        auto shadow_resources = internal::ShadowCascades();

        // 6.) Upload data...
        {
            // resource_uploader.upload_uniform_buffer(
            //     resource_tracker,
            //     BINDING_FRAME_DATA,
            //     "Frame Shader Data",
            //     "Toybox/Uniforms/Frame",
            //     frame_index,
            //     &frame_shader_data,
            //     static_cast<uint64>(sizeof(frame_shader_data))),
            // resource_uploader.upload_uniform_buffer(
            //     resource_tracker,
            //     BINDING_CAMERA_DATA,
            //     "Camera Shader Data",
            //     "Toybox/Uniforms/Camera",
            //     frame_index,
            //     &camera_shader_data,
            //     static_cast<uint64>(sizeof(camera_shader_data))),
            // resource_uploader.upload_uniform_buffer(
            //     resource_tracker,
            //     BINDING_LIGHT_DATA,
            //     "Light Shader Data",
            //     "Toybox/Uniforms/Light",
            //     frame_index,
            //     &light_shader_data,
            //     static_cast<uint64>(sizeof(light_shader_data))),
        }

        // TODO: Do the uploads inline in the above upload data section and the builds inline in the
        // above build section... be sure to comment on the uploads to explain what they are doing
        // and why and only use the Shader data stuff to upload, construct it as you upload. Don't
        // keep them around and they are in an unomptimal format for readability and maintainance on
        // the CPU side here
        // 1. Upload per-frame shader data that all render passes consume.
        //     auto frame_uniforms = internal::FrameUniformBindings();
        //     auto result = internal::upload_frame_uniform_buffers(
        //         _resource_uploader,
        //         _resource_tracker,
        //         _frame_index,
        //         _frame_shader_data,
        //         _camera_shader_data,
        //         _light_shader_data,
        //         frame_uniforms);
        //     if (!result)
        //         return {};

        //     // 2. Prepare shadow-map resources before extracting geometry so shadow casters can
        //     be
        //     // batched only when there is a valid shadow-producing light.
        //     auto shadow_resources = internal::ShadowPipelineResources();
        //     result = internal::upload_shadow_pipeline_resources(
        //         _resource_uploader,
        //         _resource_tracker,
        //         _frame_index,
        //         _shadow_pass_shader_data,
        //         _shadow_map_resolution,
        //         shadow_resources);
        //     if (!result)
        //         return {};

        // auto draw_state = internal::DrawBuildState();
        // const Vec3 camera_position = Vec3(frame.camera.position);
        // auto render_batches = internal::RenderBatchCollections();
        // render_batches = internal::collect_scene_batches(
        //     *entity_registry,
        //     _resource_uploader,
        //     shadow_resources,
        //     camera_position,
        //     _shadow_caster_max_distance);

        // result = internal::build_shadow_pass_draws(
        //     _frame_index,
        //     frame_uniforms,
        //     shadow_resources,
        //     render_batches.shadow_batches,
        //     _resource_uploader,
        //     _resource_tracker,
        //     _draw_command_factory,
        //     draw_state,
        //     passes);
        // if (!result)
        //     return {};

        // result = internal::build_scene_pass_draws(
        //     _frame_index,
        //     frame_uniforms,
        //     render_batches,
        //     _resource_uploader,
        //     _resource_tracker,
        //     _draw_command_factory,
        //     draw_state,
        //     passes);
        // if (!result)
        //     return {};

        // result = internal::build_skybox_pass_draws(
        //     _frame_index,
        //     *entity_registry,
        //     frame_uniforms,
        //     _resource_uploader,
        //     _resource_tracker,
        //     _draw_command_factory,
        //     draw_state,
        //     _sky_mesh,
        //     passes.skybox_pass);
        // if (!result)
        //     return {};

        //// 5. Append only the passes that have useful work, preserving render-pipeline order.
        // return internal::append_pipeline_passes(
        //     _resource_uploader,
        //     _resource_tracker,
        //     frame_uniforms,
        //     shadow_resources,
        //     deferred_targets,
        //     passes,
        //     out_render_passes);

        // 7.) Draw
        result = internal::execute(backend, passes);
        if (!result)
        {
            backend.end_frame();
            return result;
        }

        // 8.) Present
        if (result)
        {
            result = backend.present();
            if (!result)
            {
                backend.end_frame();
                return result;
            }
        }

        // 9.) End frame
        result = backend.end_frame();
        if (!result)
            return result;

        // 10.) Free resources and return success
        _resource_tracker.update(delta_time);
        unload_expired_resources(backend, 3.0F);
        return true;
    }
}
