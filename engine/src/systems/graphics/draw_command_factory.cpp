#include "tbx/systems/graphics/draw_command_factory.h"
#include "systems/graphics/internal/draw_command_factory_internal.h"
#include "tbx/systems/graphics/shader_bindings.h"
#include <array>
#include <string>
#include <unordered_map>
#include <vector>

namespace tbx
{
    Result RenderingDrawCommandFactory::create(
        const uint64 frame_index,
        const std::array<GraphicsResourceBinding, 3U>& frame_uniform_buffers,
        const RenderingDrawBatchInput& input,
        ResourceUploader& resource_uploader,
        RenderingResourceTracker& resource_tracker,
        std::unordered_map<uint64, RenderingMaterialUploadData>& material_uploads,
        std::unordered_map<uint64, GraphicsResourceBinding>& material_uniform_buffers,
        std::vector<GraphicsIndexedDrawCommand>& out_draw_commands) const
    {
        auto meshes = std::vector<RenderingMeshUploadData> {};
        if (input.mesh_source == RenderingMeshSourceType::DYNAMIC_RUNTIME_MESH)
        {
            if (input.dynamic_mesh)
            {
                auto mesh = RenderingMeshUploadData();
                const Result result = resource_uploader.upload_dynamic_mesh(
                    input.dynamic_mesh,
                    resource_tracker,
                    mesh);
                if (result)
                    meshes.push_back(mesh);
            }
        }
        else if (input.mesh_source == RenderingMeshSourceType::STATIC_RUNTIME_MESH)
        {
            auto mesh = RenderingMeshUploadData();
            if (resource_uploader
                    .try_get_static_runtime_mesh(input.mesh_handle, resource_tracker, mesh))
            {
                meshes.push_back(mesh);
            }
            else if (input.runtime_mesh)
            {
                const Result result = resource_uploader.upload_static_runtime_mesh(
                    input.mesh_handle,
                    *input.runtime_mesh,
                    resource_tracker,
                    mesh);
                if (result)
                    meshes.push_back(mesh);
            }
        }
        else if (input.mesh_handle.is_valid())
        {
            const Result result =
                resource_uploader.upload_model_meshes(input.mesh_handle, resource_tracker, meshes);
            if (!result)
                return result;
        }

        if (meshes.empty())
        {
            const Result result = resource_uploader.upload_fallback_mesh(resource_tracker, meshes);
            if (!result)
                return result;
        }

        const uint64 material_key = input.material_key == 0U ? input.batch_key : input.material_key;
        auto material = RenderingMaterialUploadData();
        auto result = internal::resolve_material_upload(
            material_key,
            input.material,
            resource_uploader,
            resource_tracker,
            material_uploads,
            material);
        if (!result)
            return result;

        const std::string instance_key = internal::make_draw_instance_key(input);
        auto instances = input.instances;
        if (instances.empty())
        {
            instances.push_back(
                RenderingDrawInstanceData {
                    .model_matrix = Mat4(1.0F),
                    .normal_matrix = Mat4(1.0F),
                });
        }

        const auto object_shader_data = ObjectShaderData {
            .model = instances.front().model_matrix,
            .normal_matrix = instances.front().normal_matrix,
        };
        const GraphicsResourceBinding object_uniform_buffer =
            internal::upload_object_uniform_buffer(
                frame_index,
                instance_key,
                object_shader_data,
                resource_uploader,
                resource_tracker);

        const GraphicsResourceBinding material_uniform_buffer =
            internal::upload_material_uniform_buffer(
                frame_index,
                material_key,
                material,
                resource_uploader,
                resource_tracker,
                material_uniform_buffers);

        if (!object_uniform_buffer.resource.is_valid()
            || !material_uniform_buffer.resource.is_valid())
        {
            return Result(false, "Draw command factory failed: uniform upload failed.");
        }

        const GraphicsResourceBinding instance_buffer = resource_uploader.upload_instance_buffer(
            resource_tracker,
            instance_key + "/Instances",
            frame_index,
            instances.data(),
            static_cast<uint64>(instances.size())
                * static_cast<uint64>(sizeof(RenderingDrawInstanceData)));
        if (!instance_buffer.resource.is_valid())
            return Result(false, "Draw command factory failed: instance upload failed.");

        const auto uniform_bindings = internal::make_draw_uniform_bindings(
            frame_uniform_buffers,
            object_uniform_buffer,
            material_uniform_buffer);

        for (const auto& mesh : meshes)
        {
            out_draw_commands.push_back(
                GraphicsIndexedDrawCommand {
                    .pipeline = material.pipeline,
                    .index_buffer = mesh.index_buffer,
                    .index_type = GraphicsIndexType::UINT32,
                    .vertex_buffers =
                        {GraphicsResourceBinding {
                             .slot = VERTEX_BUFFER_SLOT_MESH,
                             .resource = mesh.vertex_buffer,
                         },
                         instance_buffer},
                    .uniform_buffers = uniform_bindings,
                    .textures = material.textures,
                    .draw =
                        GraphicsDrawIndexedDesc {
                            .primitive_type = GraphicsPrimitiveType::TRIANGLES,
                            .index_type = GraphicsIndexType::UINT32,
                            .index_count = mesh.index_count,
                            .index_offset = 0U,
                            .vertex_offset = 0,
                            .instance_count = static_cast<uint32>(instances.size()),
                            .first_instance = 0U,
                        },
                });
        }

        return {};
    }
}
