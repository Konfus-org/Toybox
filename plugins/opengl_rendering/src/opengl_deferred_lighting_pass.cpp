#include "opengl_deferred_lighting_pass.h"
#include "opengl_resources/opengl_resource.h"
#include "opengl_resources/opengl_resource_manager.h"
#include "tbx/assets/builtin_assets.h"
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
        const size_t max_count = std::min(lights.size(), static_cast<size_t>(MAX_DIRECTIONAL_LIGHTS));
        for (size_t index = 0; index < max_count; ++index)
        {
            const auto& light = lights[index];
            const auto index_string = std::to_string(index);
            shader_program.try_upload(MaterialParameter {
                .name = "u_directional_lights[" + index_string + "].direction",
                .data = light.direction,
            });
            shader_program.try_upload(MaterialParameter {
                .name = "u_directional_lights[" + index_string + "].intensity",
                .data = light.intensity,
            });
            shader_program.try_upload(MaterialParameter {
                .name = "u_directional_lights[" + index_string + "].color",
                .data = light.color,
            });
            shader_program.try_upload(MaterialParameter {
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
            shader_program.try_upload(MaterialParameter {
                .name = "u_point_lights[" + index_string + "].position",
                .data = light.position,
            });
            shader_program.try_upload(MaterialParameter {
                .name = "u_point_lights[" + index_string + "].range",
                .data = light.range,
            });
            shader_program.try_upload(MaterialParameter {
                .name = "u_point_lights[" + index_string + "].color",
                .data = light.color,
            });
            shader_program.try_upload(MaterialParameter {
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
            shader_program.try_upload(MaterialParameter {
                .name = "u_spot_lights[" + index_string + "].position",
                .data = light.position,
            });
            shader_program.try_upload(MaterialParameter {
                .name = "u_spot_lights[" + index_string + "].range",
                .data = light.range,
            });
            shader_program.try_upload(MaterialParameter {
                .name = "u_spot_lights[" + index_string + "].direction",
                .data = light.direction,
            });
            shader_program.try_upload(MaterialParameter {
                .name = "u_spot_lights[" + index_string + "].inner_cos",
                .data = light.inner_cos,
            });
            shader_program.try_upload(MaterialParameter {
                .name = "u_spot_lights[" + index_string + "].color",
                .data = light.color,
            });
            shader_program.try_upload(MaterialParameter {
                .name = "u_spot_lights[" + index_string + "].outer_cos",
                .data = light.outer_cos,
            });
            shader_program.try_upload(MaterialParameter {
                .name = "u_spot_lights[" + index_string + "].intensity",
                .data = light.intensity,
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

        auto draw_resources = OpenGlDrawResources {};
        const auto lighting_material = MaterialInstance {
            .handle = deferred_lighting_material,
        };
        if (!resource_manager.try_load_post_process(lighting_material, draw_resources))
            return;
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

        draw_resources.shader_program->try_upload(MaterialParameter {
            .name = "u_gbuffer_albedo_spec",
            .data = albedo_slot,
        });
        draw_resources.shader_program->try_upload(MaterialParameter {
            .name = "u_gbuffer_normal",
            .data = normal_slot,
        });
        draw_resources.shader_program->try_upload(MaterialParameter {
            .name = "u_gbuffer_material",
            .data = material_slot,
        });
        draw_resources.shader_program->try_upload(MaterialParameter {
            .name = "u_scene_depth",
            .data = depth_slot,
        });
        draw_resources.shader_program->try_upload(MaterialParameter {
            .name = "u_camera_position",
            .data = frame_context.camera_world_position,
        });
        draw_resources.shader_program->try_upload(MaterialParameter {
            .name = "u_inverse_view_projection",
            .data = frame_context.inverse_view_projection,
        });
        draw_resources.shader_program->try_upload(MaterialParameter {
            .name = "u_directional_light_count",
            .data = std::min(
                static_cast<int>(frame_context.directional_lights.size()),
                MAX_DIRECTIONAL_LIGHTS),
        });
        draw_resources.shader_program->try_upload(MaterialParameter {
            .name = "u_point_light_count",
            .data = std::min(static_cast<int>(frame_context.point_lights.size()), MAX_POINT_LIGHTS),
        });
        draw_resources.shader_program->try_upload(MaterialParameter {
            .name = "u_spot_light_count",
            .data = std::min(static_cast<int>(frame_context.spot_lights.size()), MAX_SPOT_LIGHTS),
        });
        upload_directional_lights(*draw_resources.shader_program, frame_context.directional_lights);
        upload_point_lights(*draw_resources.shader_program, frame_context.point_lights);
        upload_spot_lights(*draw_resources.shader_program, frame_context.spot_lights);
        draw_resources.mesh->draw(draw_resources.use_tesselation);

        glBindTextureUnit(static_cast<GLuint>(albedo_slot), 0);
        glBindTextureUnit(static_cast<GLuint>(normal_slot), 0);
        glBindTextureUnit(static_cast<GLuint>(material_slot), 0);
        glBindTextureUnit(static_cast<GLuint>(depth_slot), 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glEnable(GL_DEPTH_TEST);
    }
}
