#include "GeometryPass.h"
#include "RenderPipelineFailure.h"
#include "opengl_fallbacks.h"
#include "opengl_resources/opengl_mesh.h"
#include "opengl_resources/opengl_resource_manager.h"
#include "opengl_resources/opengl_shader.h"
#include "tbx/debugging/macros.h"
#include <glad/glad.h>
#include <unordered_map>
#include <vector>

namespace opengl_rendering
{
    const auto VIEW_PROJ_UNIFORM_NAME = "u_view_proj";
    const auto MODEL_UNIFORM_NAME = "u_model";

    static GLenum to_gl_depth_function(const tbx::MaterialDepthFunction function)
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

    static void apply_depth_config(const tbx::MaterialDepthConfig& depth_config)
    {
        if (depth_config.is_test_enabled)
            glEnable(GL_DEPTH_TEST);
        else
            glDisable(GL_DEPTH_TEST);

        glDepthMask(depth_config.is_write_enabled ? GL_TRUE : GL_FALSE);
        glDepthFunc(to_gl_depth_function(depth_config.function));
    }

    static bool are_texture_bindings_equal(
        const std::vector<GLuint>& current_texture_ids,
        const std::vector<GLuint>& previous_texture_ids)
    {
        return current_texture_ids == previous_texture_ids;
    }

    static void bind_textures(
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
            && are_texture_bindings_equal(texture_ids, previous_texture_ids);
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

    GeometryPass::GeometryPass(const OpenGlResourceManager& resource_manager)
        : _resource_manager(resource_manager)
    {
    }

    GeometryPass::~GeometryPass() noexcept = default;

    void GeometryPass::draw(
        const tbx::Color& clear_color,
        const tbx::Mat4& view_proj,
        const std::vector<DrawCall>& draw_calls)
    {
        bool saw_failure = false;
        bool drew_mesh = false;

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);

        // Clear once at frame start so previously rendered batches are preserved.
        glClearColor(clear_color.r, clear_color.g, clear_color.b, clear_color.a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        auto texture_ids = std::vector<GLuint> {};
        auto previous_texture_ids = std::vector<GLuint> {};
        auto zero_texture_ids = std::vector<GLuint> {};
        std::size_t last_bound_texture_count = 0U;
        auto mesh_cache = std::unordered_map<tbx::Uuid, std::shared_ptr<OpenGlMesh>> {};
        bool is_cull_face_enabled = true;
        auto currently_bound_mesh = tbx::Uuid {};

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

            const auto should_enable_cull_face = !is_two_sided;
            if (should_enable_cull_face != is_cull_face_enabled)
            {
                if (should_enable_cull_face)
                    glEnable(GL_CULL_FACE);
                else
                    glDisable(GL_CULL_FACE);
                is_cull_face_enabled = should_enable_cull_face;
            }

            shader_program->bind();
            last_bound_texture_count = 0U;
            previous_texture_ids.clear();
            currently_bound_mesh = {};

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
                    previous_texture_ids,
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
                        material_params.material_handle.get_id().value);

                    const auto fallback_material_params =
                        create_magenta_fallback_material_params(material_params.material_handle);
                    bind_textures(
                        fallback_material_params,
                        texture_ids,
                        previous_texture_ids,
                        zero_texture_ids,
                        last_bound_texture_count);
                    if (!shader_program->try_upload(fallback_material_params))
                    {
                        saw_failure = true;
                        TBX_TRACE_WARNING(
                            "OpenGL rendering: failed to upload fallback magenta material "
                            "parameters for material '{}'.",
                            material_params.material_handle.get_id().value);
                    }
                }

                // Draw mesh
                const auto& mesh_key = meshes_ids[draw_index];
                auto mesh = std::shared_ptr<OpenGlMesh> {};
                if (const auto cached_mesh = mesh_cache.find(mesh_key);
                    cached_mesh != mesh_cache.end())
                {
                    mesh = cached_mesh->second;
                }
                else if (_resource_manager.try_get<OpenGlMesh>(mesh_key, mesh))
                {
                    mesh_cache.emplace(mesh_key, mesh);
                }
                else
                {
                    saw_failure = true;
                    TBX_TRACE_WARNING(
                        "OpenGL rendering: mesh resource '{}' is unavailable.",
                        mesh_key.value);
                    continue;
                }

                if (currently_bound_mesh != mesh_key)
                {
                    mesh->bind();
                    currently_bound_mesh = mesh_key;
                }

                const auto& material_params = material_ids[draw_index];
                const auto& render_config = material_params.render_config;
                if (render_config.depth.is_prepass_enabled)
                {
                    auto prepass_depth_config = render_config.depth;
                    prepass_depth_config.is_write_enabled = true;
                    apply_depth_config(prepass_depth_config);
                    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                    mesh->draw_bound();
                    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                }

                apply_depth_config(render_config.depth);
                mesh->draw_bound();
                drew_mesh = true;
            }
        }
        if (saw_failure && !drew_mesh)
            report_render_pipeline_failure();
    }
}
