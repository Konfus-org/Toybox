#include "GeometryPassOperation.h"
#include "OpenGlFrameContext.h"
#include "opengl_resources/opengl_mesh.h"
#include "opengl_resources/opengl_resource.h"
#include "opengl_resources/opengl_resource_manager.h"
#include "opengl_resources/opengl_shader.h"
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
        {
            glBindTextures(0, static_cast<GLsizei>(current_count), texture_ids.data());
        }

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
        const auto& [clear_color, view_proj, draw_calls] =
            std::any_cast<OpenGlFrameContext>(payload);

        // Clear once at frame start so previously rendered batches are preserved.
        glClearColor(clear_color.r, clear_color.g, clear_color.b, clear_color.a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        for (const auto& [shader_program_id, meshes_ids, material_ids, transforms] : draw_calls)
        {
            // Use shader program
            std::shared_ptr<IOpenGlResource> shader_program_resource;
            if (!_resource_manager.try_get(shader_program_id, shader_program_resource))
                continue;

            const auto shader_program =
                std::reinterpret_pointer_cast<OpenGlShaderProgram>(shader_program_resource);
            if (!shader_program)
            {
                TBX_ASSERT(false, "Shader program not found");
                continue;
            }

            auto shader_program_scope = OpenGlResourceScope(*shader_program_resource);
            {
                auto texture_ids = std::vector<GLuint> {};
                auto zero_texture_ids = std::vector<GLuint> {};
                std::size_t last_bound_texture_count = 0;

                // Update view proj
                if (!shader_program->try_upload(
                        tbx::MaterialParameter(VIEW_PROJ_UNIFORM_NAME, view_proj)))
                {
                    TBX_ASSERT(false, "Failed to upload view projection");
                    continue;
                }

                for (int draw_index = 0; draw_index < meshes_ids.size(); ++draw_index)
                {
                    // Update transform
                    if (const auto& transform = transforms[draw_index]; !shader_program->try_upload(
                            tbx::MaterialParameter(MODEL_UNIFORM_NAME, transform)))
                    {
                        TBX_ASSERT(false, "Failed to upload transform");
                        continue;
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
                        TBX_ASSERT(false, "Failed to upload material parameters");
                        continue;
                    }

                    // Draw mesh
                    {
                        const auto& mesh_key = meshes_ids[draw_index];
                        std::shared_ptr<IOpenGlResource> mesh_resource;
                        if (!_resource_manager.try_get(mesh_key, mesh_resource))
                        {
                            TBX_ASSERT(false, "Mesh not found");
                            continue;
                        }
                        auto mesh_scope = OpenGlResourceScope(*mesh_resource);
                        {
                            const auto mesh =
                                std::reinterpret_pointer_cast<OpenGlMesh>(mesh_resource);
                            if (!mesh)
                            {
                                TBX_ASSERT(false, "Mesh not found");
                                continue;
                            }
                            mesh->draw();
                        }
                    }
                }
            }
        }
    }
}
