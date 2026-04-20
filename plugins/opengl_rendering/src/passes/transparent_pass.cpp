#include "transparent_pass.h"
#include "render_pipeline_failure.h"
#include "opengl_fallbacks.h"
#include "opengl_resources/opengl_mesh.h"
#include "opengl_resources.h"
#include "opengl_resources/opengl_shader.h"
#include "tbx/debugging/macros.h"
#include <glad/glad.h>
#include <unordered_map>
#include <vector>

namespace opengl_rendering
{
    const auto TransparentViewProjUniformName = "u_view_proj";
    const auto TransparentModelUniformName = "u_model";

    static GLenum transparent_to_gl_depth_function(const tbx::MaterialDepthFunction function)
    {
        switch (function)
        {
            case tbx::MaterialDepthFunction::Less:
                return GL_LESS;
            case tbx::MaterialDepthFunction::LessEqual:
                return GL_LEQUAL;
            case tbx::MaterialDepthFunction::Always:
                return GL_ALWAYS;
            default:
                return GL_LESS;
        }
    }

    static void apply_transparent_depth_config(const tbx::MaterialDepthConfig& depth_config)
    {
        if (depth_config.is_test_enabled)
            glEnable(GL_DEPTH_TEST);
        else
            glDisable(GL_DEPTH_TEST);

        glDepthMask(depth_config.is_write_enabled ? GL_TRUE : GL_FALSE);
        glDepthFunc(transparent_to_gl_depth_function(depth_config.function));
    }

    static bool are_transparent_texture_bindings_equal(
        const std::vector<GLuint>& current_texture_ids,
        const std::vector<GLuint>& previous_texture_ids)
    {
        return current_texture_ids == previous_texture_ids;
    }

    static void bind_transparent_textures(
        const OpenGlMaterialParams& material,
        std::vector<GLuint>& texture_ids,
        std::vector<GLuint>& previous_texture_ids,
        std::vector<GLuint>& zero_texture_ids,
        std::size_t& last_bound_count)
    {
        texture_ids.clear();
        texture_ids.reserve(material.textures.size());
        for (const auto& texture : material.textures)
            texture_ids.push_back(static_cast<GLuint>(texture.gl_texture_id));

        const auto current_count = texture_ids.size();
        const auto layout_matches_previous =
            current_count == last_bound_count
            && are_transparent_texture_bindings_equal(texture_ids, previous_texture_ids);
        if (!layout_matches_previous && current_count > 0)
            glBindTextures(0, static_cast<GLsizei>(current_count), texture_ids.data());

        if (!layout_matches_previous && last_bound_count > current_count)
        {
            const auto extra_count = last_bound_count - current_count;
            zero_texture_ids.assign(extra_count, 0);
            glBindTextures(
                static_cast<GLuint>(current_count),
                static_cast<GLsizei>(extra_count),
                zero_texture_ids.data());
        }

        previous_texture_ids = texture_ids;
        last_bound_count = current_count;
    }

    TransparentPass::TransparentPass(
        const OpenGlResources& resources,
        OpenGlGBuffer& gbuffer)
        : _resources(resources)
        , _gbuffer(gbuffer)
    {
    }

    TransparentPass::~TransparentPass() noexcept = default;

    void TransparentPass::draw(
        const tbx::Mat4& view_projection,
        const std::vector<TransparentDrawCall>& draw_calls)
    {
        if (draw_calls.empty())
            return;

        bool saw_failure = false;
        bool drew_mesh = false;

        _gbuffer.bind();
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glDepthFunc(GL_LESS);
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);

        auto texture_ids = std::vector<GLuint> {};
        auto previous_texture_ids = std::vector<GLuint> {};
        auto zero_texture_ids = std::vector<GLuint> {};
        std::size_t last_bound_texture_count = 0U;
        auto shader_cache = std::unordered_map<tbx::Uuid, std::shared_ptr<OpenGlShaderProgram>> {};
        auto mesh_cache = std::unordered_map<tbx::Uuid, std::shared_ptr<OpenGlMesh>> {};
        auto currently_bound_shader = tbx::Uuid {};
        auto currently_bound_mesh = tbx::Uuid {};
        bool is_cull_face_enabled = true;

        for (const auto& draw_call : draw_calls)
        {
            auto shader_program = std::shared_ptr<OpenGlShaderProgram>();
            if (const auto cached_shader = shader_cache.find(draw_call.shader_program);
                cached_shader != shader_cache.end())
            {
                shader_program = cached_shader->second;
            }
            else if (_resources.try_get<OpenGlShaderProgram>(
                         draw_call.shader_program,
                         shader_program))
            {
                shader_cache.emplace(draw_call.shader_program, shader_program);
            }
            else
            {
                saw_failure = true;
                TBX_TRACE_WARNING(
                    "OpenGL rendering: shader program '{}' is unavailable for transparent pass.",
                    draw_call.shader_program.value);
                continue;
            }

            const auto should_enable_cull_face = !draw_call.is_two_sided;
            if (should_enable_cull_face != is_cull_face_enabled)
            {
                if (should_enable_cull_face)
                    glEnable(GL_CULL_FACE);
                else
                    glDisable(GL_CULL_FACE);
                is_cull_face_enabled = should_enable_cull_face;
            }

            if (currently_bound_shader != draw_call.shader_program)
            {
                shader_program->bind();
                currently_bound_shader = draw_call.shader_program;
                currently_bound_mesh = {};
                last_bound_texture_count = 0U;
                previous_texture_ids.clear();

                if (!shader_program->try_upload(
                        tbx::MaterialParameter(
                            TransparentViewProjUniformName,
                            view_projection)))
                {
                    saw_failure = true;
                    TBX_TRACE_WARNING(
                        "OpenGL rendering: failed to upload view projection uniform to "
                        "transparent program '{}'.",
                        draw_call.shader_program.value);
                }
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

            bind_transparent_textures(
                draw_call.material,
                texture_ids,
                previous_texture_ids,
                zero_texture_ids,
                last_bound_texture_count);
            if (!shader_program->try_upload(draw_call.material))
            {
                saw_failure = true;
                TBX_TRACE_WARNING(
                    "OpenGL rendering: failed to upload transparent material parameters for "
                    "material '{}'. Using fallback magenta material parameters.",
                    draw_call.material.material_handle.get_id().value);

                const auto fallback_material_params =
                    create_magenta_fallback_material_params(draw_call.material.material_handle);
                bind_transparent_textures(
                    fallback_material_params,
                    texture_ids,
                    previous_texture_ids,
                    zero_texture_ids,
                    last_bound_texture_count);
                if (!shader_program->try_upload(fallback_material_params))
                {
                    saw_failure = true;
                    TBX_TRACE_WARNING(
                        "OpenGL rendering: failed to upload fallback transparent material "
                        "parameters for material '{}'.",
                        draw_call.material.material_handle.get_id().value);
                }
            }

            auto mesh = std::shared_ptr<OpenGlMesh> {};
            if (const auto cached_mesh = mesh_cache.find(draw_call.mesh); cached_mesh != mesh_cache.end())
            {
                mesh = cached_mesh->second;
            }
            else if (_resources.try_get<OpenGlMesh>(draw_call.mesh, mesh))
            {
                mesh_cache.emplace(draw_call.mesh, mesh);
            }
            else
            {
                saw_failure = true;
                TBX_TRACE_WARNING(
                    "OpenGL rendering: mesh resource '{}' is unavailable for transparent pass.",
                    draw_call.mesh.value);
                continue;
            }

            if (currently_bound_mesh != draw_call.mesh)
            {
                mesh->bind();
                currently_bound_mesh = draw_call.mesh;
            }

            apply_transparent_depth_config(draw_call.material.render_config.depth);
            mesh->draw_bound();
            drew_mesh = true;
        }

        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
        glDisable(GL_BLEND);
        glEnable(GL_CULL_FACE);
        if (saw_failure && !drew_mesh)
            report_render_pipeline_failure();
    }
}
