#include "GeometryPassOperation.h"
#include "OpenGlFrameContext.h"
#include "RenderPipelineFailure.h"
#include "opengl_fallbacks.h"
#include "opengl_resources/opengl_mesh.h"
#include "opengl_resources/opengl_resource.h"
#include "opengl_resources/opengl_resource_manager.h"
#include "opengl_resources/opengl_shader.h"
#include "tbx/debugging/macros.h"
#include <glad/glad.h>
#include <vector>

namespace opengl_rendering
{
    const auto VIEW_PROJ_UNIFORM_NAME = "u_view_proj";
    const auto MODEL_UNIFORM_NAME = "u_model";

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

    GeometryPassOperation::GeometryPassOperation(const OpenGlResourceManager& resource_manager)
        : _resource_manager(resource_manager)
    {
    }

    GeometryPassOperation::~GeometryPassOperation() = default;

    void GeometryPassOperation::execute(const std::any& payload)
    {
        const auto& frame_context = std::any_cast<const OpenGlFrameContext&>(payload);
        const auto& clear_color = frame_context.clear_color;
        const auto& view_proj = frame_context.view_projection;
        const auto& draw_calls = frame_context.draw_calls;
        bool saw_failure = false;
        bool drew_mesh = false;

        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);

        // Clear once at frame start so previously rendered batches are preserved.
        glClearColor(clear_color.r, clear_color.g, clear_color.b, clear_color.a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        for (const auto& [shader_program_id, is_two_sided, meshes_ids, material_ids, transforms] :
             draw_calls)
        {
            // Use shader program
            auto shader_program = std::shared_ptr<OpenGlShaderProgram>();
            if (!_resource_manager.try_get<OpenGlShaderProgram>(shader_program_id, shader_program))
            {
                saw_failure = true;
                TBX_TRACE_WARNING(
                    "OpenGL rendering: shader program '{}' is unavailable for geometry pass.",
                    shader_program_id.value);
                continue;
            }

            if (is_two_sided)
                glDisable(GL_CULL_FACE);
            else
                glEnable(GL_CULL_FACE);

            auto shader_program_scope = OpenGlResourceScope(*shader_program);
            {
                auto texture_ids = std::vector<GLuint> {};
                auto zero_texture_ids = std::vector<GLuint> {};
                std::size_t last_bound_texture_count = 0;

                // Update view proj
                if (!shader_program->try_upload(
                        tbx::MaterialParameter(VIEW_PROJ_UNIFORM_NAME, view_proj)))
                {
                    saw_failure = true;
                    TBX_ASSERT(false, "Failed to upload view projection");
                    TBX_TRACE_WARNING(
                        "OpenGL rendering: failed to upload view projection uniform to program "
                        "'{}'. Continuing with previous/default value.",
                        shader_program_id.value);
                }

                for (std::size_t draw_index = 0; draw_index < meshes_ids.size(); ++draw_index)
                {
                    // Update transform
                    if (const auto& transform = transforms[draw_index]; !shader_program->try_upload(
                            tbx::MaterialParameter(MODEL_UNIFORM_NAME, transform)))
                    {
                        saw_failure = true;
                        TBX_TRACE_WARNING(
                            "OpenGL rendering: failed to upload transform for draw index {} on "
                            "program '{}'. Using identity transform fallback.",
                            draw_index,
                            shader_program_id.value);
                        shader_program->try_upload(
                            tbx::MaterialParameter(MODEL_UNIFORM_NAME, tbx::Mat4(1.0F)));
                    }

                    bind_textures(
                        material_ids[draw_index],
                        texture_ids,
                        zero_texture_ids,
                        last_bound_texture_count);

                    // Upload material params
                    if (const auto& material_params = material_ids[draw_index];
                        !shader_program->try_upload(material_params))
                    {
                        saw_failure = true;
                        TBX_TRACE_WARNING(
                            "OpenGL rendering: failed to upload material parameters for material "
                            "'{}'. Using fallback magenta material parameters.",
                            material_params.material_handle.id.value);

                        const auto fallback_material_params =
                            create_magenta_fallback_material_params(
                                material_params.material_handle);
                        bind_textures(
                            fallback_material_params,
                            texture_ids,
                            zero_texture_ids,
                            last_bound_texture_count);
                        if (!shader_program->try_upload(fallback_material_params))
                        {
                            saw_failure = true;
                            TBX_TRACE_WARNING(
                                "OpenGL rendering: failed to upload fallback magenta material "
                                "parameters for material '{}'.",
                                material_params.material_handle.id.value);
                        }
                    }

                    // Draw mesh
                    {
                        const auto& mesh_key = meshes_ids[draw_index];
                        auto mesh = std::shared_ptr<OpenGlMesh> {};
                        if (!_resource_manager.try_get<OpenGlMesh>(mesh_key, mesh))
                        {
                            saw_failure = true;
                            TBX_TRACE_WARNING(
                                "OpenGL rendering: mesh resource '{}' is unavailable.",
                                mesh_key.value);
                            continue;
                        }
                        auto mesh_scope = OpenGlResourceScope(*mesh);
                        {
                            mesh->draw();
                            drew_mesh = true;
                        }
                    }
                }
            }
        }
        if (saw_failure && !drew_mesh)
            report_render_pipeline_failure();
    }
}
