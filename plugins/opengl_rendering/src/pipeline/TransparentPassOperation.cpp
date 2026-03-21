#include "TransparentPassOperation.h"
#include "RenderPipelineFailure.h"
#include "opengl_fallbacks.h"
#include "opengl_resources/opengl_mesh.h"
#include "opengl_resources/opengl_resource.h"
#include "opengl_resources/opengl_shader.h"
#include "tbx/debugging/macros.h"
#include <glad/glad.h>
#include <vector>

namespace opengl_rendering
{
    const auto TransparentViewProjUniformName = "u_view_proj";
    const auto TransparentModelUniformName = "u_model";

    static void bind_textures(
        const OpenGlMaterialParams& material,
        std::vector<GLuint>& texture_ids,
        std::vector<GLuint>& zero_texture_ids,
        std::size_t& last_bound_count)
    {
        texture_ids.clear();
        texture_ids.reserve(material.textures.size());
        for (const auto& texture : material.textures)
        {
            if (texture.gl_texture_id == 0)
                continue;
            texture_ids.push_back(static_cast<GLuint>(texture.gl_texture_id));
        }

        const auto current_count = texture_ids.size();
        if (current_count > 0)
            glBindTextures(0, static_cast<GLsizei>(current_count), texture_ids.data());

        if (last_bound_count > current_count)
        {
            const auto extra_count = last_bound_count - current_count;
            zero_texture_ids.assign(extra_count, 0);
            glBindTextures(
                static_cast<GLuint>(current_count),
                static_cast<GLsizei>(extra_count),
                zero_texture_ids.data());
        }

        last_bound_count = current_count;
    }

    TransparentPassOperation::TransparentPassOperation(
        const OpenGlResourceManager& resource_manager,
        OpenGlGBuffer& gbuffer)
        : _resource_manager(resource_manager)
        , _gbuffer(gbuffer)
    {
    }

    TransparentPassOperation::~TransparentPassOperation() noexcept = default;

    void TransparentPassOperation::execute(const std::any& payload)
    {
        const auto& frame_context = std::any_cast<const OpenGlFrameContext&>(payload);
        if (frame_context.transparent_draw_calls.empty())
            return;

        bool saw_failure = false;
        bool drew_mesh = false;

        auto gbuffer_scope = OpenGlResourceScope(_gbuffer);

        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);

        auto texture_ids = std::vector<GLuint> {};
        auto zero_texture_ids = std::vector<GLuint> {};
        std::size_t last_bound_texture_count = 0U;

        for (const auto& draw_call : frame_context.transparent_draw_calls)
        {
            auto shader_program = std::shared_ptr<OpenGlShaderProgram>();
            if (!_resource_manager.try_get<OpenGlShaderProgram>(
                    draw_call.shader_program,
                    shader_program))
            {
                saw_failure = true;
                TBX_TRACE_WARNING(
                    "OpenGL rendering: shader program '{}' is unavailable for transparent pass.",
                    draw_call.shader_program.value);
                continue;
            }

            if (draw_call.is_two_sided)
                glDisable(GL_CULL_FACE);
            else
                glEnable(GL_CULL_FACE);

            auto shader_program_scope = OpenGlResourceScope(*shader_program);
            if (!shader_program->try_upload(
                    tbx::MaterialParameter(
                        TransparentViewProjUniformName,
                        frame_context.view_projection)))
            {
                saw_failure = true;
                TBX_TRACE_WARNING(
                    "OpenGL rendering: failed to upload view projection uniform to transparent "
                    "program '{}'.",
                    draw_call.shader_program.value);
            }
            if (!shader_program->try_upload(
                    tbx::MaterialParameter(TransparentModelUniformName, draw_call.transform)))
            {
                saw_failure = true;
                TBX_TRACE_WARNING(
                    "OpenGL rendering: failed to upload model transform for transparent program "
                    "'{}'.",
                    draw_call.shader_program.value);
            }

            bind_textures(
                draw_call.material,
                texture_ids,
                zero_texture_ids,
                last_bound_texture_count);
            if (!shader_program->try_upload(draw_call.material))
            {
                saw_failure = true;
                TBX_TRACE_WARNING(
                    "OpenGL rendering: failed to upload transparent material parameters for "
                    "material '{}'. Using fallback magenta material parameters.",
                    draw_call.material.material_handle.id.value);

                const auto fallback_material_params =
                    create_magenta_fallback_material_params(draw_call.material.material_handle);
                bind_textures(
                    fallback_material_params,
                    texture_ids,
                    zero_texture_ids,
                    last_bound_texture_count);
                if (!shader_program->try_upload(fallback_material_params))
                {
                    saw_failure = true;
                    TBX_TRACE_WARNING(
                        "OpenGL rendering: failed to upload fallback transparent material "
                        "parameters for material '{}'.",
                        draw_call.material.material_handle.id.value);
                }
            }

            auto mesh = std::shared_ptr<OpenGlMesh> {};
            if (!_resource_manager.try_get<OpenGlMesh>(draw_call.mesh, mesh))
            {
                saw_failure = true;
                TBX_TRACE_WARNING(
                    "OpenGL rendering: mesh resource '{}' is unavailable for transparent pass.",
                    draw_call.mesh.value);
                continue;
            }

            auto mesh_scope = OpenGlResourceScope(*mesh);
            mesh->draw();
            drew_mesh = true;
        }

        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glEnable(GL_CULL_FACE);
        if (saw_failure && !drew_mesh)
            report_render_pipeline_failure();
    }
}
