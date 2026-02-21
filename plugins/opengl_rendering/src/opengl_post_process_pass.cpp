#include "opengl_post_process_pass.h"
#include "opengl_resources/opengl_resource.h"
#include "opengl_resources/opengl_resource_manager.h"
#include "tbx/debugging/macros.h"
#include <algorithm>
#include <glad/glad.h>
#include <vector>

namespace tbx::plugins
{
    static void get_preset_destination_bounds(
        const Size& source_size,
        const Size& destination_size,
        const OpenGlFrameBufferPresentMode mode,
        GLint& out_x,
        GLint& out_y,
        GLint& out_width,
        GLint& out_height)
    {
        out_x = 0;
        out_y = 0;
        out_width = static_cast<GLint>(destination_size.width);
        out_height = static_cast<GLint>(destination_size.height);
        if (mode == OpenGlFrameBufferPresentMode::STRETCH)
            return;

        const auto source_width = static_cast<float>(source_size.width);
        const auto source_height = static_cast<float>(source_size.height);
        const auto destination_width = static_cast<float>(destination_size.width);
        const auto destination_height = static_cast<float>(destination_size.height);
        const auto source_aspect = source_width / source_height;
        const auto destination_aspect = destination_width / destination_height;

        if (source_aspect > destination_aspect)
        {
            out_height = static_cast<GLint>(destination_width / source_aspect);
            out_y = (static_cast<GLint>(destination_size.height) - out_height) / 2;
            return;
        }

        if (source_aspect < destination_aspect)
        {
            out_width = static_cast<GLint>(destination_height * source_aspect);
            out_x = (static_cast<GLint>(destination_size.width) - out_width) / 2;
        }
    }

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

    static void draw_effect(
        const OpenGlRenderFrameContext& frame_context,
        const OpenGlFrameBuffer& source_target,
        const OpenGlDrawResources& draw_resources,
        const OpenGlPostProcessEffect& effect)
    {
        auto resource_scopes = std::vector<GlResourceScope> {};
        resource_scopes.reserve(draw_resources.textures.size() + 2);
        resource_scopes.push_back(GlResourceScope(*draw_resources.shader_program));
        resource_scopes.push_back(GlResourceScope(*draw_resources.mesh));
        bind_textures(draw_resources, resource_scopes);

        const int scene_color_texture_slot = static_cast<int>(draw_resources.textures.size());
        uint32 scene_color_texture_id = source_target.get_color_texture_id();
        if (
            frame_context.lighting_target != nullptr
            && (&source_target == frame_context.lighting_target)
            && frame_context.scene_color_texture_id != 0)
        {
            scene_color_texture_id = frame_context.scene_color_texture_id;
        }
        glBindTextureUnit(
            static_cast<GLuint>(scene_color_texture_slot),
            scene_color_texture_id);
        int texture_slot = scene_color_texture_slot + 1;
        int scene_depth_texture_slot = -1;
        int gbuffer_albedo_spec_texture_slot = -1;
        int gbuffer_normal_texture_slot = -1;
        int gbuffer_material_texture_slot = -1;
        if (frame_context.gbuffer != nullptr)
        {
            scene_depth_texture_slot = texture_slot++;
            gbuffer_albedo_spec_texture_slot = texture_slot++;
            gbuffer_normal_texture_slot = texture_slot++;
            gbuffer_material_texture_slot = texture_slot++;

            glBindTextureUnit(
                static_cast<GLuint>(scene_depth_texture_slot),
                frame_context.gbuffer->get_depth_texture_id());
            glBindTextureUnit(
                static_cast<GLuint>(gbuffer_albedo_spec_texture_slot),
                frame_context.gbuffer->get_albedo_spec_texture_id());
            glBindTextureUnit(
                static_cast<GLuint>(gbuffer_normal_texture_slot),
                frame_context.gbuffer->get_normal_texture_id());
            glBindTextureUnit(
                static_cast<GLuint>(gbuffer_material_texture_slot),
                frame_context.gbuffer->get_material_texture_id());
        }

        const float clamped_blend = std::clamp(effect.blend, 0.0f, 1.0f);
        draw_resources.shader_program->try_upload(MaterialParameter {
            .name = "u_scene_color",
            .data = scene_color_texture_slot,
        });
        if (scene_depth_texture_slot >= 0)
        {
            draw_resources.shader_program->try_upload(MaterialParameter {
                .name = "u_scene_depth",
                .data = scene_depth_texture_slot,
            });
            draw_resources.shader_program->try_upload(MaterialParameter {
                .name = "u_gbuffer_albedo_spec",
                .data = gbuffer_albedo_spec_texture_slot,
            });
            draw_resources.shader_program->try_upload(MaterialParameter {
                .name = "u_gbuffer_normal",
                .data = gbuffer_normal_texture_slot,
            });
            draw_resources.shader_program->try_upload(MaterialParameter {
                .name = "u_gbuffer_material",
                .data = gbuffer_material_texture_slot,
            });
        }
        draw_resources.shader_program->try_upload(MaterialParameter {
            .name = "u_blend",
            .data = clamped_blend,
        });
        for (const auto& uniform : draw_resources.shader_parameters)
            draw_resources.shader_program->try_upload(uniform);

        draw_resources.mesh->draw(draw_resources.use_tesselation);
        glBindTextureUnit(static_cast<GLuint>(scene_color_texture_slot), 0);
        if (scene_depth_texture_slot >= 0)
        {
            glBindTextureUnit(static_cast<GLuint>(scene_depth_texture_slot), 0);
            glBindTextureUnit(static_cast<GLuint>(gbuffer_albedo_spec_texture_slot), 0);
            glBindTextureUnit(static_cast<GLuint>(gbuffer_normal_texture_slot), 0);
            glBindTextureUnit(static_cast<GLuint>(gbuffer_material_texture_slot), 0);
        }
    }

    static void draw_to_intermediate_target(
        const OpenGlRenderFrameContext& frame_context,
        OpenGlFrameBuffer& destination_target,
        const OpenGlFrameBuffer& source_target,
        const OpenGlDrawResources& draw_resources,
        const OpenGlPostProcessEffect& effect)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, destination_target.get_framebuffer_id());
        const auto resolution = destination_target.get_resolution();
        glViewport(
            0,
            0,
            static_cast<GLsizei>(resolution.width),
            static_cast<GLsizei>(resolution.height));
        glClear(GL_COLOR_BUFFER_BIT);
        draw_effect(frame_context, source_target, draw_resources, effect);
    }

    static void draw_to_present_target(
        const OpenGlRenderFrameContext& frame_context,
        const OpenGlFrameBuffer& source_target,
        const OpenGlDrawResources& draw_resources,
        const OpenGlPostProcessEffect& effect)
    {
        auto destination_x = GLint {};
        auto destination_y = GLint {};
        auto destination_width = GLint {};
        auto destination_height = GLint {};
        get_preset_destination_bounds(
            source_target.get_resolution(),
            frame_context.viewport_size,
            frame_context.present_mode,
            destination_x,
            destination_y,
            destination_width,
            destination_height);

        glBindFramebuffer(GL_FRAMEBUFFER, frame_context.present_target_framebuffer_id);
        glViewport(
            0,
            0,
            static_cast<GLsizei>(frame_context.viewport_size.width),
            static_cast<GLsizei>(frame_context.viewport_size.height));
        glClear(GL_COLOR_BUFFER_BIT);
        glViewport(destination_x, destination_y, destination_width, destination_height);
        draw_effect(frame_context, source_target, draw_resources, effect);
    }

    void OpenGlPostProcessPass::execute(
        const OpenGlRenderFrameContext& frame_context,
        OpenGlResourceManager& resource_manager) const
    {
        TBX_ASSERT(
            frame_context.lighting_target != nullptr,
            "OpenGL rendering: post-process pass requires a lighting target.");
        TBX_ASSERT(
            frame_context.post_process_ping_target != nullptr,
            "OpenGL rendering: post-process pass requires a ping framebuffer.");
        TBX_ASSERT(
            frame_context.post_process_pong_target != nullptr,
            "OpenGL rendering: post-process pass requires a pong framebuffer.");

        const auto& post_process = frame_context.post_process;
        if (!post_process.is_enabled || post_process.effects.empty())
        {
            frame_context.lighting_target->preset(
                frame_context.present_target_framebuffer_id,
                frame_context.viewport_size,
                frame_context.present_mode);
            return;
        }

        auto enabled_effects = std::vector<OpenGlPostProcessEffect> {};
        enabled_effects.reserve(post_process.effects.size());
        for (const auto& effect : post_process.effects)
        {
            if (!effect.is_enabled)
                continue;
            if (!effect.material.handle.is_valid())
                continue;

            enabled_effects.push_back(effect);
        }

        if (enabled_effects.empty())
        {
            frame_context.lighting_target->preset(
                frame_context.present_target_framebuffer_id,
                frame_context.viewport_size,
                frame_context.present_mode);
            return;
        }

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_BLEND);

        const OpenGlFrameBuffer* source_target = frame_context.lighting_target;
        OpenGlFrameBuffer* ping_target = frame_context.post_process_ping_target;
        OpenGlFrameBuffer* pong_target = frame_context.post_process_pong_target;
        bool did_draw_effect = false;

        for (size_t effect_index = 0; effect_index < enabled_effects.size(); ++effect_index)
        {
            const auto& effect = enabled_effects[effect_index];
            auto draw_resources = OpenGlDrawResources {};
            if (!resource_manager.try_load_post_process(effect.material, draw_resources))
                continue;
            if (!draw_resources.mesh || !draw_resources.shader_program)
                continue;

            const bool is_last_effect = effect_index == (enabled_effects.size() - 1U);
            if (is_last_effect)
            {
                draw_to_present_target(frame_context, *source_target, draw_resources, effect);
                did_draw_effect = true;
                continue;
            }

            OpenGlFrameBuffer* destination_target =
                (effect_index % 2U == 0U) ? ping_target : pong_target;
            draw_to_intermediate_target(
                frame_context,
                *destination_target,
                *source_target,
                draw_resources,
                effect);
            source_target = destination_target;
            did_draw_effect = true;
        }

        if (!did_draw_effect)
        {
            frame_context.lighting_target->preset(
                frame_context.present_target_framebuffer_id,
                frame_context.viewport_size,
                frame_context.present_mode);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glEnable(GL_DEPTH_TEST);
    }
}
