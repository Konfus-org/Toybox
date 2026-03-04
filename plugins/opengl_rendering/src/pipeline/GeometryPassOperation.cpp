#include "GeometryPassOperation.h"
#include "OpenGlFrameContext.h"
#include "opengl_resources/opengl_mesh.h"
#include "opengl_resources/opengl_resource.h"
#include "opengl_resources/opengl_resource_manager.h"
#include "opengl_resources/opengl_shader.h"
#include <cstddef>
#include <future>
#include <glad/glad.h>
#include <vector>

namespace opengl_rendering
{
    const auto VIEW_PROJ_UNIFORM_NAME = "u_view_proj";

    struct DrawBatch
    {
        tbx::Uuid mesh_id = {};
        int first_draw_index = 0;
        std::vector<int> draw_indices = {};
    };

    static bool are_materials_equal(
        const OpenGlMaterialParams& left,
        const OpenGlMaterialParams& right)
    {
        return left.material_handle.id == right.material_handle.id;
    }

    static std::vector<DrawBatch> build_batches(
        const std::vector<tbx::Uuid>& meshes,
        const std::vector<OpenGlMaterialParams>& materials)
    {
        auto batches = std::vector<DrawBatch> {};
        for (int draw_index = 0; draw_index < static_cast<int>(meshes.size()); ++draw_index)
        {
            bool merged = false;
            for (auto& [mesh_id, first_draw_index, draw_indices] : batches)
            {
                if (mesh_id != meshes[draw_index])
                    continue;
                if (!are_materials_equal(materials[first_draw_index], materials[draw_index]))
                    continue;

                draw_indices.push_back(draw_index);
                merged = true;
                break;
            }

            if (merged)
                continue;

            auto batch = DrawBatch();
            batch.mesh_id = meshes[draw_index];
            batch.first_draw_index = draw_index;
            batch.draw_indices.push_back(draw_index);
            batches.push_back(std::move(batch));
        }

        return batches;
    }

    static void bind_textures(
        const OpenGlMaterialParams& material,
        std::vector<GLuint>& texture_ids,
        std::vector<GLuint>& zero_texture_ids,
        std::size_t& last_bound_count)
    {
        bool all_bindless = !material.textures.empty();
        for (const auto& texture : material.textures)
        {
            if (texture.bindless_handle == 0)
            {
                all_bindless = false;
                break;
            }
        }

        if (all_bindless)
        {
            if (last_bound_count > 0)
            {
                zero_texture_ids.assign(last_bound_count, 0);
                glBindTextures(0, static_cast<GLsizei>(last_bound_count), zero_texture_ids.data());
                last_bound_count = 0;
            }
            return;
        }

        texture_ids.clear();
        texture_ids.reserve(material.textures.size());
        for (const auto& texture : material.textures)
        {
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

    GeometryPassOperation::GeometryPassOperation(
        const OpenGlResourceManager& resource_manager,
        tbx::JobSystem& job_system)
        : _job_system(job_system)
        , _resource_manager(resource_manager)
    {
    }

    GeometryPassOperation::~GeometryPassOperation() = default;

    void GeometryPassOperation::execute(const std::any& payload)
    {
        const auto& [clear_color, view_proj, draw_calls] =
            std::any_cast<OpenGlFrameContext>(payload);
        auto batch_futures = std::vector<std::future<std::vector<DrawBatch>>> {};
        batch_futures.reserve(draw_calls.size());
        for (const auto& draw_call : draw_calls)
        {
            batch_futures.push_back(_job_system.schedule_with_future(
                [&draw_call]
                {
                    return build_batches(draw_call.meshes, draw_call.materials);
                }));
        }

        // Clear once at frame start so previously rendered batches are preserved.
        glClearColor(clear_color.r, clear_color.g, clear_color.b, clear_color.a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        for (std::size_t draw_call_index = 0; draw_call_index < draw_calls.size();
             ++draw_call_index)
        {
            const auto& [shader_program_id, meshes_ids, material_ids, transforms, instance_ids] =
                draw_calls[draw_call_index];

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

                // Draw batches
                for (const auto batches = batch_futures[draw_call_index].get();
                     const auto& [mesh_id, first_draw_index, draw_indices] : batches)
                {
                    const auto representative_draw = first_draw_index;
                    bind_textures(
                        material_ids[representative_draw],
                        texture_ids,
                        zero_texture_ids,
                        last_bound_texture_count);

                    if (!shader_program->try_upload_material_block(
                            material_ids[representative_draw]))
                    {
                        TBX_ASSERT(
                            false,
                            "Failed to upload material block. Program: {}, draw index: {}",
                            shader_program->get_program_id(),
                            representative_draw);
                        continue;
                    }

                    if (!shader_program->try_upload(material_ids[representative_draw]))
                    {
                        TBX_ASSERT(
                            false,
                            "Failed to upload material parameters. Program: {}, draw index: {}",
                            shader_program->get_program_id(),
                            representative_draw);
                        continue;
                    }

                    std::shared_ptr<IOpenGlResource> mesh_resource;
                    if (!_resource_manager.try_get(mesh_id, mesh_resource))
                    {
                        TBX_ASSERT(false, "Mesh not found");
                        continue;
                    }
                    auto mesh_scope = OpenGlResourceScope(*mesh_resource);
                    const auto mesh = std::reinterpret_pointer_cast<OpenGlMesh>(mesh_resource);
                    if (!mesh)
                    {
                        TBX_ASSERT(false, "Mesh not found");
                        continue;
                    }

                    auto instance_data = std::vector<OpenGlMeshInstanceData> {};
                    instance_data.reserve(draw_indices.size());
                    for (const auto draw_index : draw_indices)
                    {
                        const auto& instance_id = draw_index < static_cast<int>(instance_ids.size())
                                                      ? instance_ids[draw_index]
                                                      : tbx::Uuid {};
                        instance_data.push_back(
                            OpenGlMeshInstanceData {
                                .model = transforms[draw_index],
                                .instance_id = static_cast<tbx::uint32>(instance_id),
                            });
                    }

                    mesh->upload_instance_data(
                        instance_data,
                        shader_program->get_instance_model_attribute_location(),
                        shader_program->get_instance_id_attribute_location());
                    mesh->draw_instanced(static_cast<tbx::uint32>(instance_data.size()));
                }
            }
        }
    }
}
