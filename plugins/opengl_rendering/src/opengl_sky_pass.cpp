#include "opengl_sky_pass.h"
#include "opengl_resources/opengl_resource.h"
#include "opengl_resources/opengl_resource_manager.h"
#include "tbx/debugging/macros.h"
#include "tbx/graphics/camera.h"
#include "tbx/math/transform.h"
#include <glad/glad.h>
#include <vector>

namespace tbx::plugins
{
    static Quat get_camera_rotation(const OpenGlCameraView& camera_view)
    {
        if (!camera_view.camera_entity.has_component<Transform>())
            return Quat(1.0f, 0.0f, 0.0f, 0.0f);

        const auto& camera_transform = camera_view.camera_entity.get_component<Transform>();
        return camera_transform.rotation;
    }

    static Vec3 get_camera_position(const OpenGlCameraView& camera_view)
    {
        if (!camera_view.camera_entity.has_component<Transform>())
            return Vec3(0.0f);

        const auto& camera_transform = camera_view.camera_entity.get_component<Transform>();
        return camera_transform.position;
    }

    static Mat4 get_sky_view_projection_matrix(
        const OpenGlCameraView& camera_view,
        const Size& render_resolution)
    {
        if (!camera_view.camera_entity.has_component<Camera>())
            return Mat4(1.0f);

        auto& camera = camera_view.camera_entity.get_component<Camera>();
        camera.set_aspect(render_resolution.get_aspect_ratio());
        const auto camera_rotation = get_camera_rotation(camera_view);
        const auto camera_position = get_camera_position(camera_view);

        auto sky_view = camera.get_view_matrix(camera_position, camera_rotation);
        sky_view[3][0] = 0.0f;
        sky_view[3][1] = 0.0f;
        sky_view[3][2] = 0.0f;
        return camera.get_projection_matrix() * sky_view;
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

    static void upload_material_uniforms(
        OpenGlShaderProgram& shader_program,
        const OpenGlDrawResources& draw_resources)
    {
        for (const auto& uniform : draw_resources.shader_parameters)
            shader_program.try_upload(uniform);
    }

    void OpenGlSkyPass::execute(
        const OpenGlRenderFrameContext& frame_context,
        OpenGlResourceManager& resource_manager) const
    {
        TBX_ASSERT(
            frame_context.gbuffer != nullptr,
            "OpenGL rendering: sky pass requires a gbuffer target.");

        auto render_target_scope = GlResourceScope(*frame_context.gbuffer);
        glViewport(
            0,
            0,
            static_cast<GLsizei>(frame_context.render_resolution.width),
            static_cast<GLsizei>(frame_context.render_resolution.height));
        glDisable(GL_BLEND);
        frame_context.gbuffer->clear(frame_context.clear_color);

        if (!frame_context.sky_material.handle.is_valid())
            return;

        auto draw_resources = OpenGlDrawResources {};
        if (!resource_manager.try_load_sky(frame_context.sky_material, draw_resources))
            return;

        if (!draw_resources.mesh || !draw_resources.shader_program)
            return;

        TBX_ASSERT(
            draw_resources.shader_program->get_program_id() != 0,
            "OpenGL rendering: sky pass requires a valid shader program.");

        const auto sky_view_projection =
            get_sky_view_projection_matrix(
                frame_context.camera_view,
                frame_context.render_resolution);

        auto resource_scopes = std::vector<GlResourceScope> {};
        resource_scopes.reserve(draw_resources.textures.size() + 2);
        resource_scopes.push_back(GlResourceScope(*draw_resources.shader_program));
        resource_scopes.push_back(GlResourceScope(*draw_resources.mesh));
        bind_textures(draw_resources, resource_scopes);

        draw_resources.shader_program->upload(MaterialParameter {
            .name = "u_view_proj",
            .data = sky_view_projection,
        });
        draw_resources.shader_program->upload(MaterialParameter {
            .name = "u_model",
            .data = Mat4(1.0f),
        });
        upload_material_uniforms(*draw_resources.shader_program, draw_resources);

        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);
        draw_resources.mesh->draw(draw_resources.use_tesselation);
        glDepthMask(GL_TRUE);
    }
}
