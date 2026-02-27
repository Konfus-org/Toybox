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

namespace opengl_rendering
{
    static constexpr int MAX_DIRECTIONAL_CASCADE_SPLITS = 4;
    static constexpr int MAX_SHADOW_MAPS = 24;

    static void bind_textures(
        const OpenGlDrawResources& draw_resources,
        std::vector<GlResourceScope>& out_scopes)
    {
        for (const auto& texture_binding : draw_resources.textures)
        {
            if (!texture_binding.texture)
                continue;

            texture_binding.texture->set_slot(static_cast<uint32>(texture_binding.slot));
            out_scopes.push_back(GlResourceScope(*texture_binding.texture));
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
        GLint max_texture_units = 0;
        glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_texture_units);
        const int available_texture_slots =
            std::max(0, max_texture_units - std::max(first_texture_slot, 0));
        const auto shadow_map_count = std::min(
            max_shadow_maps,
            std::min(
                static_cast<size_t>(MAX_SHADOW_MAPS),
                static_cast<size_t>(available_texture_slots)));

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
        shader_program.try_upload(
            MaterialParameter {
                .name = "u_shadow_softness",
                .data = frame_context.shadow_data.shadow_softness,
            });

        const auto split_count = std::min(
            frame_context.shadow_data.cascade_splits.size(),
            static_cast<size_t>(MAX_DIRECTIONAL_CASCADE_SPLITS));
        for (size_t index = 0; index < split_count; ++index)
        {
            shader_program.try_upload(
                MaterialParameter {
                    .name = "u_cascade_splits[" + std::to_string(index) + "]",
                    .data = frame_context.shadow_data.cascade_splits[index],
                });
        }
    }

    static void bind_light_buffers(const OpenGlRenderFrameContext& frame_context)
    {
        glBindBufferBase(
            GL_SHADER_STORAGE_BUFFER,
            0U,
            frame_context.light_culling.packed_lights_buffer_id);
        glBindBufferBase(
            GL_SHADER_STORAGE_BUFFER,
            1U,
            frame_context.light_culling.tile_headers_buffer_id);
        glBindBufferBase(
            GL_SHADER_STORAGE_BUFFER,
            2U,
            frame_context.light_culling.tile_light_indices_buffer_id);
    }

    static void unbind_light_buffers()
    {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0U, 0U);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1U, 0U);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2U, 0U);
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
        if (!resource_manager.try_get(frame_context.deferred_lighting_entity, deferred_lighting_resource))
            return;

        auto deferred_lighting_stack =
            std::dynamic_pointer_cast<OpenGlPostProcessingStackResource>(deferred_lighting_resource);
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
        for (const auto& texture_binding : draw_resources.textures)
        {
            draw_resources.shader_program->try_upload(
                MaterialParameter {
                    .name = texture_binding.uniform_name,
                    .data = texture_binding.slot,
                });
        }

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
                .name = "u_camera_forward",
                .data = frame_context.camera_forward,
            });
        draw_resources.shader_program->try_upload(
            MaterialParameter {
                .name = "u_inverse_view_projection",
                .data = frame_context.inverse_view_projection,
            });
        draw_resources.shader_program->try_upload(
            MaterialParameter {
                .name = "u_render_resolution",
                .data = tbx::Vec2(
                    static_cast<float>(frame_context.render_resolution.width),
                    static_cast<float>(frame_context.render_resolution.height)),
            });
        draw_resources.shader_program->try_upload(
            MaterialParameter {
                .name = "u_tile_size",
                .data = static_cast<int>(frame_context.light_culling.tile_size),
            });
        draw_resources.shader_program->try_upload(
            MaterialParameter {
                .name = "u_tile_count_x",
                .data = static_cast<int>(frame_context.light_culling.tile_count_x),
            });
        draw_resources.shader_program->try_upload(
            MaterialParameter {
                .name = "u_tile_count_y",
                .data = static_cast<int>(frame_context.light_culling.tile_count_y),
            });
        draw_resources.shader_program->try_upload(
            MaterialParameter {
                .name = "u_packed_light_count",
                .data = static_cast<int>(frame_context.light_culling.packed_light_count),
            });
        draw_resources.shader_program->try_upload(
            MaterialParameter {
                .name = "u_compute_culling_enabled",
                .data = frame_context.is_compute_culling_enabled,
            });
        draw_resources.shader_program->try_upload(
            MaterialParameter {
                .name = "u_include_local_lights",
                .data = !frame_context.is_local_light_volume_enabled,
            });

        const int first_shadow_map_slot = texture_slot;
        size_t bound_shadow_map_count = 0;
        upload_shadow_data(
            *draw_resources.shader_program,
            frame_context,
            resource_manager,
            first_shadow_map_slot,
            bound_shadow_map_count);

        bind_light_buffers(frame_context);
        draw_resources.mesh->draw(draw_resources.use_tesselation);
        unbind_light_buffers();

        glBindTextureUnit(static_cast<GLuint>(albedo_slot), 0);
        glBindTextureUnit(static_cast<GLuint>(normal_slot), 0);
        glBindTextureUnit(static_cast<GLuint>(material_slot), 0);
        glBindTextureUnit(static_cast<GLuint>(depth_slot), 0);
        for (size_t shadow_index = 0; shadow_index < bound_shadow_map_count; ++shadow_index)
        {
            const auto shadow_texture_slot = first_shadow_map_slot + static_cast<int>(shadow_index);
            glBindTextureUnit(static_cast<GLuint>(shadow_texture_slot), 0);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glEnable(GL_DEPTH_TEST);
    }
}
