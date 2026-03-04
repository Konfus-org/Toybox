#include "GeometryPassOperation.h"
#include "OpenGlFrameContext.h"
#include "opengl_resources/opengl_mesh.h"
#include "opengl_resources/opengl_resource.h"
#include "opengl_resources/opengl_resource_manager.h"
#include "opengl_resources/opengl_shader.h"
#include <cstddef>
#include <cstring>
#include <glad/glad.h>
#include <variant>
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

    static bool are_uniform_data_equal(const tbx::UniformData& left, const tbx::UniformData& right)
    {
        return std::visit(
            []<typename T0, typename T1>(const T0& left_value, const T1& right_value) -> bool
            {
                using LeftType = std::decay_t<T0>;
                using RightType = std::decay_t<T1>;
                if constexpr (!std::is_same_v<LeftType, RightType>)
                {
                    return false;
                }
                else if constexpr (std::is_same_v<LeftType, tbx::Vec2>)
                {
                    return left_value.x == right_value.x && left_value.y == right_value.y;
                }
                else if constexpr (std::is_same_v<LeftType, tbx::Vec3>)
                {
                    return left_value.x == right_value.x && left_value.y == right_value.y
                           && left_value.z == right_value.z;
                }
                else if constexpr (std::is_same_v<LeftType, tbx::Vec4>)
                {
                    return left_value.x == right_value.x && left_value.y == right_value.y
                           && left_value.z == right_value.z && left_value.w == right_value.w;
                }
                else if constexpr (std::is_same_v<LeftType, tbx::Color>)
                {
                    return left_value.r == right_value.r && left_value.g == right_value.g
                           && left_value.b == right_value.b && left_value.a == right_value.a;
                }
                else if constexpr (std::is_same_v<LeftType, tbx::Mat3>)
                {
                    for (int col = 0; col < 3; ++col)
                    {
                        for (int row = 0; row < 3; ++row)
                        {
                            if (left_value[col][row] != right_value[col][row])
                                return false;
                        }
                    }
                    return true;
                }
                else if constexpr (std::is_same_v<LeftType, tbx::Mat4>)
                {
                    for (int col = 0; col < 4; ++col)
                    {
                        for (int row = 0; row < 4; ++row)
                        {
                            if (left_value[col][row] != right_value[col][row])
                                return false;
                        }
                    }
                    return true;
                }
                else
                {
                    return left_value == right_value;
                }
            },
            left,
            right);
    }

    static bool are_materials_equal(
        const OpenGlMaterialParams& left,
        const OpenGlMaterialParams& right)
    {
        if (left.parameters.size() != right.parameters.size())
            return false;
        if (left.textures.size() != right.textures.size())
            return false;

        for (std::size_t parameter_index = 0; parameter_index < left.parameters.size();
             ++parameter_index)
        {
            const auto& [name_a, data_a] = left.parameters[parameter_index];
            const auto& [name_b, data_b] = right.parameters[parameter_index];
            if (name_a != name_b)
                return false;
            if (!are_uniform_data_equal(data_a, data_b))
                return false;
        }

        for (std::size_t texture_index = 0; texture_index < left.textures.size(); ++texture_index)
        {
            const auto& left_texture = left.textures[texture_index];
            const auto& right_texture = right.textures[texture_index];
            if (left_texture.name != right_texture.name)
                return false;
            if (left_texture.texture_id != right_texture.texture_id)
                return false;
            if (left_texture.bindless_handle != right_texture.bindless_handle)
                return false;
        }

        return true;
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

        for (const auto& [shader_program_id, meshes_ids, material_ids, transforms, instance_ids] :
             draw_calls)
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

                // Draw batches
                for (const auto batches = build_batches(meshes_ids, material_ids);
                     const auto& [mesh_id, first_draw_index, draw_indices] : batches)
                {
                    const auto representative_draw = first_draw_index;
                    bind_textures(
                        material_ids[representative_draw],
                        texture_ids,
                        zero_texture_ids,
                        last_bound_texture_count);

                    if (!shader_program->try_upload(material_ids[representative_draw]))
                    {
                        TBX_ASSERT(false, "Failed to upload material parameters");
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
