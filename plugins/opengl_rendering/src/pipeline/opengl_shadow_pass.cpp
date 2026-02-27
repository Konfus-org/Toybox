#include "opengl_shadow_pass.h"
#include "opengl_resources/opengl_resource.h"
#include "opengl_resources/opengl_resource_manager.h"
#include "opengl_resources/opengl_shadow_map.h"
#include "tbx/debugging/macros.h"
#include "tbx/ecs/entity.h"
#include "tbx/graphics/renderer.h"
#include "tbx/math/vectors.h"
#include <algorithm>
#include <glad/glad.h>
#include <vector>

namespace opengl_rendering
{
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

    static void upload_texture_uniforms(
        OpenGlShaderProgram& shader_program,
        const OpenGlDrawResources& draw_resources)
    {
        for (const auto& texture_binding : draw_resources.textures)
        {
            shader_program.try_upload(
                MaterialParameter {
                    .name = texture_binding.uniform_name,
                    .data = texture_binding.slot,
                });
        }
    }

    static void upload_model_uniform(OpenGlShaderProgram& shader_program, const tbx::Entity& entity)
    {
        const auto transform = get_world_space_transform(entity);
        const auto model_matrix = build_transform_matrix(transform);
        shader_program.upload(
            MaterialParameter {
                .name = "u_model",
                .data = model_matrix,
            });
    }

    static void upload_light_view_projection(
        OpenGlShaderProgram& shader_program,
        const tbx::Mat4& light_view_projection)
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
        const tbx::Entity& entity,
        OpenGlResourceManager& resource_manager,
        const tbx::Mat4& light_view_projection,
        std::vector<GlResourceScope>& resource_scopes,
        const tbx::Vec3& camera_position,
        float shadow_render_distance)
    {
        if (!entity.has_component<Renderer>())
            return;

        const auto& renderer = entity.get_component<Renderer>();
        if (renderer.shadow_mode == ShadowMode::None)
            return;

        if (renderer.shadow_mode == ShadowMode::Standard && shadow_render_distance > 0.0F)
        {
            const auto entity_transform = get_world_space_transform(entity);
            const float distance_to_camera = length(entity_transform.position - camera_position);
            if (distance_to_camera > shadow_render_distance)
                return;
        }

        auto draw_resources = OpenGlDrawResources {};
        if (!resource_manager.try_get(entity, draw_resources))
            return;
        if (!draw_resources.mesh || !draw_resources.shader_program)
            return;

        resource_scopes.clear();
        resource_scopes.reserve(draw_resources.textures.size() + 2);
        resource_scopes.push_back(GlResourceScope(*draw_resources.shader_program));
        resource_scopes.push_back(GlResourceScope(*draw_resources.mesh));
        bind_textures(draw_resources, resource_scopes);

        upload_light_view_projection(*draw_resources.shader_program, light_view_projection);
        upload_texture_uniforms(*draw_resources.shader_program, draw_resources);
        upload_material_uniforms(*draw_resources.shader_program, draw_resources);
        upload_model_uniform(*draw_resources.shader_program, entity);
        draw_resources.mesh->draw(draw_resources.use_tesselation);
    }

    OpenGlShadowPass::~OpenGlShadowPass() noexcept
    {
        if (_shadow_framebuffer_id == 0)
            return;

        glDeleteFramebuffers(1, &_shadow_framebuffer_id);
        _shadow_framebuffer_id = 0;
    }

    uint32 OpenGlShadowPass::get_or_create_shadow_framebuffer_id()
    {
        if (_shadow_framebuffer_id != 0)
            return _shadow_framebuffer_id;

        glCreateFramebuffers(1, &_shadow_framebuffer_id);
        if (_shadow_framebuffer_id == 0)
            return 0;

        glNamedFramebufferDrawBuffer(_shadow_framebuffer_id, GL_NONE);
        glNamedFramebufferReadBuffer(_shadow_framebuffer_id, GL_NONE);
        return _shadow_framebuffer_id;
    }

    void OpenGlShadowPass::execute(
        const OpenGlRenderFrameContext& frame_context,
        OpenGlResourceManager& resource_manager)
    {
        const auto max_shadow_maps = std::min(
            frame_context.shadow_data.map_uuids.size(),
            frame_context.shadow_data.light_view_projections.size());
        const auto shadow_map_count = max_shadow_maps;
        if (shadow_map_count == 0)
            return;

        const uint32 shadow_framebuffer_id = get_or_create_shadow_framebuffer_id();
        if (shadow_framebuffer_id == 0)
            return;

        glBindFramebuffer(GL_FRAMEBUFFER, shadow_framebuffer_id);

        const auto shadow_map_resolution =
            static_cast<GLsizei>(std::max(1U, frame_context.shadow_data.shadow_map_resolution));
        glViewport(0, 0, shadow_map_resolution, shadow_map_resolution);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(2.5f, 8.0f);

        auto resource_scopes = std::vector<GlResourceScope> {};
        for (size_t shadow_index = 0; shadow_index < shadow_map_count; ++shadow_index)
        {
            const auto shadow_map_uuid = frame_context.shadow_data.map_uuids[shadow_index];
            auto base_resource = std::shared_ptr<IOpenGlResource> {};
            if (!resource_manager.try_get(shadow_map_uuid, base_resource))
            {
                TBX_ASSERT(
                    false,
                    "OpenGL rendering: shadow map UUID was not registered in resource manager.");
                continue;
            }
            auto shadow_map = std::dynamic_pointer_cast<OpenGlShadowMap>(base_resource);
            if (!shadow_map)
            {
                TBX_ASSERT(
                    false,
                    "OpenGL rendering: shadow map UUID was not registered in resource manager.");
                continue;
            }

            const auto texture_id = shadow_map->get_texture_id();
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
            const float shadow_render_distance =
                std::max(0.0F, frame_context.shadow_data.shadow_render_distance);
            for (const auto& entity : frame_context.camera_view.in_view_static_entities)
                draw_shadow_entity(
                    entity,
                    resource_manager,
                    light_view_projection,
                    resource_scopes,
                    frame_context.camera_world_position,
                    shadow_render_distance);
            for (const auto& entity : frame_context.camera_view.in_view_dynamic_entities)
                draw_shadow_entity(
                    entity,
                    resource_manager,
                    light_view_projection,
                    resource_scopes,
                    frame_context.camera_world_position,
                    shadow_render_distance);
        }

        glDisable(GL_POLYGON_OFFSET_FILL);
        glDisable(GL_CULL_FACE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
}
