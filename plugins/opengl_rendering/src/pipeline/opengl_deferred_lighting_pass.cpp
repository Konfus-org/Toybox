#include "opengl_deferred_lighting_pass.h"
#include "opengl_resources/opengl_post_processing_stack_resource.h"
#include "opengl_resources/opengl_resource.h"
#include "opengl_resources/opengl_resource_manager.h"
#include "opengl_resources/opengl_shadow_map.h"
#include "tbx/debugging/macros.h"
#include <algorithm>
#include <glad/glad.h>
#include <string>
#include <vector>

namespace tbx::plugins
{
    static constexpr int MAX_DIRECTIONAL_LIGHTS = 4;
    static constexpr int MAX_POINT_LIGHTS = 32;
    static constexpr int MAX_SPOT_LIGHTS = 16;
    static constexpr int MAX_SHADOW_MAPS = 4;

    static void bind_textures(
        const OpenGlDrawResources& draw_resources,
        std::vector<GlResourceScope>& out_scopes)
    {
        for (const auto& texture_binding : draw_resources.textures)
        {
            if (!texture_binding.texture)
                continue;

            out_scopes.push_back(GlResourceScope(*texture_binding.texture));
        }
    }

    static void upload_directional_lights(
        OpenGlShaderProgram& shader_program,
        std::span<const OpenGlDirectionalLightData> lights)
    {
        const size_t max_count =
            std::min(lights.size(), static_cast<size_t>(MAX_DIRECTIONAL_LIGHTS));
        for (size_t index = 0; index < max_count; ++index)
        {
            const auto& light = lights[index];
            const auto index_string = std::to_string(index);
            shader_program.try_upload(
                MaterialParameter {
                    .name = "u_directional_lights[" + index_string + "].direction",
                    .data = light.direction,
                });
            shader_program.try_upload(
                MaterialParameter {
                    .name = "u_directional_lights[" + index_string + "].intensity",
                    .data = light.intensity,
                });
            shader_program.try_upload(
                MaterialParameter {
                    .name = "u_directional_lights[" + index_string + "].color",
                    .data = light.color,
                });
            shader_program.try_upload(
                MaterialParameter {
                    .name = "u_directional_lights[" + index_string + "].ambient",
                    .data = light.ambient,
                });
        }
    }

    static void upload_point_lights(
        OpenGlShaderProgram& shader_program,
        std::span<const OpenGlPointLightData> lights)
    {
        const size_t max_count = std::min(lights.size(), static_cast<size_t>(MAX_POINT_LIGHTS));
        for (size_t index = 0; index < max_count; ++index)
        {
            const auto& light = lights[index];
            const auto index_string = std::to_string(index);
            shader_program.try_upload(
                MaterialParameter {
                    .name = "u_point_lights[" + index_string + "].position",
                    .data = light.position,
                });
            shader_program.try_upload(
                MaterialParameter {
                    .name = "u_point_lights[" + index_string + "].range",
                    .data = light.range,
                });
            shader_program.try_upload(
                MaterialParameter {
                    .name = "u_point_lights[" + index_string + "].color",
                    .data = light.color,
                });
            shader_program.try_upload(
                MaterialParameter {
                    .name = "u_point_lights[" + index_string + "].intensity",
                    .data = light.intensity,
                });
        }
    }

    static void upload_spot_lights(
        OpenGlShaderProgram& shader_program,
        std::span<const OpenGlSpotLightData> lights)
    {
        const size_t max_count = std::min(lights.size(), static_cast<size_t>(MAX_SPOT_LIGHTS));
        for (size_t index = 0; index < max_count; ++index)
        {
            const auto& light = lights[index];
            const auto index_string = std::to_string(index);
            shader_program.try_upload(
                MaterialParameter {
                    .name = "u_spot_lights[" + index_string + "].position",
                    .data = light.position,
                });
            shader_program.try_upload(
                MaterialParameter {
                    .name = "u_spot_lights[" + index_string + "].range",
                    .data = light.range,
                });
            shader_program.try_upload(
                MaterialParameter {
                    .name = "u_spot_lights[" + index_string + "].direction",
                    .data = light.direction,
                });
            shader_program.try_upload(
                MaterialParameter {
                    .name = "u_spot_lights[" + index_string + "].inner_cos",
                    .data = light.inner_cos,
                });
            shader_program.try_upload(
                MaterialParameter {
                    .name = "u_spot_lights[" + index_string + "].color",
                    .data = light.color,
                });
            shader_program.try_upload(
                MaterialParameter {
                    .name = "u_spot_lights[" + index_string + "].outer_cos",
                    .data = light.outer_cos,
                });
            shader_program.try_upload(
                MaterialParameter {
                    .name = "u_spot_lights[" + index_string + "].intensity",
                    .data = light.intensity,
                });
        }
    }

    static void upload_shadow_data(
        OpenGlShaderProgram& shader_program,
        const OpenGlRenderFrameContext& frame_context,
        OpenGlResourceManager& resource_manager,
        int first_texture_slot,
        size_t& out_bound_shadow_map_count)
    {
        const auto max_shadow_maps = std::min(
            frame_context.shadow_data.map_uuids.size(),
            frame_context.shadow_data.light_view_projections.size());
        const auto shadow_map_count =
            std::min(max_shadow_maps, static_cast<size_t>(MAX_SHADOW_MAPS));

        out_bound_shadow_map_count = 0;
        shader_program.try_upload(
            MaterialParameter {
                .name = "u_shadow_map_count",
                .data = 0,
            });

        for (size_t index = 0; index < shadow_map_count; ++index)
        {
            const auto index_string = std::to_string(index);
            const auto texture_slot = first_texture_slot + static_cast<int>(index);
            const auto shadow_map_uuid = frame_context.shadow_data.map_uuids[index];
            auto base_resource = std::shared_ptr<IOpenGlResource> {};
            if (!resource_manager.try_get(shadow_map_uuid, base_resource))
            {
                TBX_ASSERT(
                    false,
                    "OpenGL rendering: deferred pass could not resolve shadow-map UUID.");
                continue;
            }
            auto shadow_map = std::dynamic_pointer_cast<OpenGlShadowMap>(base_resource);
            if (!shadow_map)
            {
                TBX_ASSERT(
                    false,
                    "OpenGL rendering: deferred pass could not resolve shadow-map UUID.");
                continue;
            }

            const auto texture_id = shadow_map->get_texture_id();
            if (texture_id == 0)
                continue;

            glBindTextureUnit(static_cast<GLuint>(texture_slot), texture_id);

            shader_program.try_upload(
                MaterialParameter {
                    .name = "u_shadow_maps[" + index_string + "]",
                    .data = texture_slot,
                });
            shader_program.try_upload(
                MaterialParameter {
                    .name = "u_light_view_projection_matrices[" + index_string + "]",
                    .data = frame_context.shadow_data.light_view_projections[index],
                });
            out_bound_shadow_map_count += 1;
        }
        shader_program.try_upload(
            MaterialParameter {
                .name = "u_shadow_map_count",
                .data = static_cast<int>(out_bound_shadow_map_count),
            });

        const auto split_count = std::min(
            frame_context.shadow_data.cascade_splits.size(),
            static_cast<size_t>(MAX_SHADOW_MAPS));
        for (size_t index = 0; index < split_count; ++index)
        {
            shader_program.try_upload(
                MaterialParameter {
                    .name = "u_cascade_splits[" + std::to_string(index) + "]",
                    .data = frame_context.shadow_data.cascade_splits[index],
                });
        }
    }

    void OpenGlDeferredLightingPass::execute(
        const OpenGlRenderFrameContext& frame_context,
        OpenGlResourceManager& resource_manager) const
    {
        TBX_ASSERT(
            frame_context.gbuffer != nullptr,
            "OpenGL rendering: deferred pass requires a valid G-buffer.");
        TBX_ASSERT(
            frame_context.lighting_target != nullptr,
            "OpenGL rendering: deferred pass requires a lighting target.");

        auto deferred_lighting_resource = std::shared_ptr<IOpenGlResource> {};
        if (!resource_manager.try_get(
                frame_context.deferred_lighting_entity,
                deferred_lighting_resource))
            return;

        auto deferred_lighting_stack = std::dynamic_pointer_cast<OpenGlPostProcessingStackResource>(
            deferred_lighting_resource);
        if (!deferred_lighting_stack || deferred_lighting_stack->get_effects().empty())
            return;

        const auto& draw_resources = deferred_lighting_stack->get_effects()[0].draw_resources;
        if (!draw_resources.mesh || !draw_resources.shader_program)
            return;

        glBindFramebuffer(GL_FRAMEBUFFER, frame_context.lighting_target->get_framebuffer_id());
        const auto lighting_resolution = frame_context.lighting_target->get_resolution();
        glViewport(
            0,
            0,
            static_cast<GLsizei>(lighting_resolution.width),
            static_cast<GLsizei>(lighting_resolution.height));
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        auto resource_scopes = std::vector<GlResourceScope> {};
        resource_scopes.reserve(draw_resources.textures.size() + 2);
        resource_scopes.push_back(GlResourceScope(*draw_resources.shader_program));
        resource_scopes.push_back(GlResourceScope(*draw_resources.mesh));
        bind_textures(draw_resources, resource_scopes);

        int texture_slot = static_cast<int>(draw_resources.textures.size());
        const int albedo_slot = texture_slot++;
        const int normal_slot = texture_slot++;
        const int material_slot = texture_slot++;
        const int depth_slot = texture_slot++;

        glBindTextureUnit(
            static_cast<GLuint>(albedo_slot),
            frame_context.gbuffer->get_albedo_spec_texture_id());
        glBindTextureUnit(
            static_cast<GLuint>(normal_slot),
            frame_context.gbuffer->get_normal_texture_id());
        glBindTextureUnit(
            static_cast<GLuint>(material_slot),
            frame_context.gbuffer->get_material_texture_id());
        glBindTextureUnit(
            static_cast<GLuint>(depth_slot),
            frame_context.gbuffer->get_depth_texture_id());

        draw_resources.shader_program->try_upload(
            MaterialParameter {
                .name = "u_gbuffer_albedo_spec",
                .data = albedo_slot,
            });
        draw_resources.shader_program->try_upload(
            MaterialParameter {
                .name = "u_gbuffer_normal",
                .data = normal_slot,
            });
        draw_resources.shader_program->try_upload(
            MaterialParameter {
                .name = "u_gbuffer_material",
                .data = material_slot,
            });
        draw_resources.shader_program->try_upload(
            MaterialParameter {
                .name = "u_scene_depth",
                .data = depth_slot,
            });
        draw_resources.shader_program->try_upload(
            MaterialParameter {
                .name = "u_camera_position",
                .data = frame_context.camera_world_position,
            });
        draw_resources.shader_program->try_upload(
            MaterialParameter {
                .name = "u_inverse_view_projection",
                .data = frame_context.inverse_view_projection,
            });
        const int first_shadow_map_slot = texture_slot;
        size_t bound_shadow_map_count = 0;
        upload_shadow_data(
            *draw_resources.shader_program,
            frame_context,
            resource_manager,
            first_shadow_map_slot,
            bound_shadow_map_count);

        draw_resources.shader_program->try_upload(
            MaterialParameter {
                .name = "u_directional_light_count",
                .data = std::min(
                    static_cast<int>(frame_context.directional_lights.size()),
                    MAX_DIRECTIONAL_LIGHTS),
            });
        draw_resources.shader_program->try_upload(
            MaterialParameter {
                .name = "u_point_light_count",
                .data =
                    std::min(static_cast<int>(frame_context.point_lights.size()), MAX_POINT_LIGHTS),
            });
        draw_resources.shader_program->try_upload(
            MaterialParameter {
                .name = "u_spot_light_count",
                .data =
                    std::min(static_cast<int>(frame_context.spot_lights.size()), MAX_SPOT_LIGHTS),
            });
        upload_directional_lights(*draw_resources.shader_program, frame_context.directional_lights);
        upload_point_lights(*draw_resources.shader_program, frame_context.point_lights);
        upload_spot_lights(*draw_resources.shader_program, frame_context.spot_lights);
        draw_resources.mesh->draw(draw_resources.use_tesselation);

        glBindTextureUnit(static_cast<GLuint>(albedo_slot), 0);
        glBindTextureUnit(static_cast<GLuint>(normal_slot), 0);
        glBindTextureUnit(static_cast<GLuint>(material_slot), 0);
        glBindTextureUnit(static_cast<GLuint>(depth_slot), 0);
        const auto max_shadow_maps = std::min(
            frame_context.shadow_data.map_uuids.size(),
            frame_context.shadow_data.light_view_projections.size());
        const auto shadow_map_count =
            std::min(max_shadow_maps, static_cast<size_t>(MAX_SHADOW_MAPS));
        for (size_t shadow_index = 0; shadow_index < shadow_map_count; ++shadow_index)
        {
            const auto shadow_texture_slot = first_shadow_map_slot + static_cast<int>(shadow_index);
            glBindTextureUnit(static_cast<GLuint>(shadow_texture_slot), 0);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glEnable(GL_DEPTH_TEST);
    }
}
