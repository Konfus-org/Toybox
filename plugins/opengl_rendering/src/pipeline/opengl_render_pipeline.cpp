#include "opengl_render_pipeline.h"
#include "OpenGlFrameContext.h"
#include "glad/glad.h"
#include "opengl_resources/opengl_mesh.h"
#include "opengl_resources/opengl_resource.h"
#include "opengl_resources/opengl_shader.h"
#include "tbx/debugging/macros.h"
#include <any>

namespace opengl_rendering
{
    const auto VIEW_PROJ_UNIFORM_NAME = "u_view_proj";
    const auto MODEL_UNIFORM_NAME = "u_model";

    OpenGlRenderPipeline::OpenGlRenderPipeline(const OpenGlResourceManager& resource_manager)
        : _resource_manager(resource_manager)
    {
    }

    OpenGlRenderPipeline::~OpenGlRenderPipeline() noexcept
    {
        clear_operations();
    }

    void OpenGlRenderPipeline::execute(const std::any& payload)
    {
        const auto& [clear_color, view_proj, draw_calls] =
            std::any_cast<OpenGlFrameContext>(payload);

        for (const auto& [shader_program_id, meshes_ids, material_ids, transforms] : draw_calls)
        {
            // Clear
            glClearColor(clear_color.r, clear_color.g, clear_color.b, clear_color.a);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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
