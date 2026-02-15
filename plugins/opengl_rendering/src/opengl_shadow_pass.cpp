#include "opengl_shadow_pass.h"
#include "opengl_resources/opengl_resource.h"
#include "opengl_resources/opengl_resource_manager.h"
#include "tbx/debugging/macros.h"
#include "tbx/graphics/renderer.h"
#include "tbx/math/transform.h"
#include <algorithm>
#include <glad/glad.h>
#include <vector>

namespace tbx::plugins
{
    static constexpr size_t MAX_SHADOW_MAPS = 4;
    static constexpr uint32 SHADOW_MAP_RESOLUTION = 2048;

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

    static void upload_model_uniform(OpenGlShaderProgram& shader_program, const Entity& entity)
    {
        auto transform = Transform {};
        if (entity.has_component<Transform>())
            transform = entity.get_component<Transform>();

        const auto model_matrix = build_transform_matrix(transform);
        shader_program.upload(
            MaterialParameter {
                .name = "u_model",
                .data = model_matrix,
            });
    }

    static void upload_light_view_projection(
        OpenGlShaderProgram& shader_program,
        const Mat4& light_view_projection)
    {
        shader_program.try_upload(
            MaterialParameter {
                .name = "u_light_view_proj",
                .data = light_view_projection,
            });
        shader_program.try_upload(
            MaterialParameter {
                .name = "u_view_proj",
                .data = light_view_projection,
            });
    }

    static void upload_material_uniforms(
        OpenGlShaderProgram& shader_program,
        const OpenGlDrawResources& draw_resources)
    {
        for (const auto& uniform : draw_resources.shader_parameters)
            shader_program.try_upload(uniform);
    }

    static void draw_shadow_entity(
        const Entity& entity,
        OpenGlResourceManager& resource_manager,
        const Mat4& light_view_projection)
    {
        if (!entity.has_component<Renderer>())
            return;

        const auto& renderer = entity.get_component<Renderer>();
        if (!renderer.are_shadows_enabled)
            return;

        auto draw_resources = OpenGlDrawResources {};
        if (!resource_manager.try_load(entity, draw_resources))
            return;
        if (!draw_resources.mesh || !draw_resources.shader_program)
            return;

        auto resource_scopes = std::vector<GlResourceScope> {};
        resource_scopes.reserve(draw_resources.textures.size() + 2);
        resource_scopes.push_back(GlResourceScope(*draw_resources.shader_program));
        resource_scopes.push_back(GlResourceScope(*draw_resources.mesh));
        bind_textures(draw_resources, resource_scopes);

        upload_light_view_projection(*draw_resources.shader_program, light_view_projection);
        upload_material_uniforms(*draw_resources.shader_program, draw_resources);
        upload_model_uniform(*draw_resources.shader_program, entity);
        draw_resources.mesh->draw(draw_resources.use_tesselation);
    }

    void OpenGlShadowPass::execute(
        const OpenGlRenderFrameContext& frame_context,
        OpenGlResourceManager& resource_manager) const
    {
        const auto max_shadow_maps = std::min(
            frame_context.shadow_data.map_texture_ids.size(),
            frame_context.shadow_data.light_view_projections.size());
        const auto shadow_map_count = std::min(max_shadow_maps, MAX_SHADOW_MAPS);
        if (shadow_map_count == 0)
            return;

        uint32 shadow_framebuffer_id = 0;
        glCreateFramebuffers(1, &shadow_framebuffer_id);
        if (shadow_framebuffer_id == 0)
            return;

        glNamedFramebufferDrawBuffer(shadow_framebuffer_id, GL_NONE);
        glNamedFramebufferReadBuffer(shadow_framebuffer_id, GL_NONE);
        glBindFramebuffer(GL_FRAMEBUFFER, shadow_framebuffer_id);

        glViewport(0, 0, SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);

        for (size_t shadow_index = 0; shadow_index < shadow_map_count; ++shadow_index)
        {
            const auto texture_id = frame_context.shadow_data.map_texture_ids[shadow_index];
            if (texture_id == 0)
                continue;

            glNamedFramebufferTexture(shadow_framebuffer_id, GL_DEPTH_ATTACHMENT, texture_id, 0);

            const auto status =
                glCheckNamedFramebufferStatus(shadow_framebuffer_id, GL_FRAMEBUFFER);
            if (status != GL_FRAMEBUFFER_COMPLETE)
                continue;

            glClear(GL_DEPTH_BUFFER_BIT);
            const auto& light_view_projection =
                frame_context.shadow_data.light_view_projections[shadow_index];
            for (const auto& entity : frame_context.camera_view.in_view_static_entities)
                draw_shadow_entity(entity, resource_manager, light_view_projection);
            for (const auto& entity : frame_context.camera_view.in_view_dynamic_entities)
                draw_shadow_entity(entity, resource_manager, light_view_projection);
        }

        glCullFace(GL_BACK);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &shadow_framebuffer_id);
    }
}
