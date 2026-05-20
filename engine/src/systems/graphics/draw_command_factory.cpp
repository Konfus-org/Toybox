#include "tbx/systems/graphics/draw_command_factory.h"
#include "tbx/systems/graphics/shader_bindings.h"
#include <string>
#include <vector>

namespace tbx::detail
{
    static std::string make_draw_instance_key(const RenderingDrawCommandInput& input)
    {
        if (!input.instance_key.empty())
            return input.instance_key;

        return std::string("Toybox/Draw/") + to_string(input.handle);
    }
}

namespace tbx
{
    Result RenderingDrawCommandFactory::create(
        const uint64 frame_index,
        const std::array<GraphicsResourceBinding, 3U>& frame_uniform_buffers,
        const RenderingDrawCommandInput& input,
        ResourceUploader& resource_uploader,
        RenderingResourceTracker& resource_tracker,
        std::vector<GraphicsIndexedDrawCommand>& out_draw_commands) const
    {
        auto meshes = std::vector<RenderingMeshUploadData> {};
        if (input.type == RenderingDrawCommandInputType::DYNAMIC)
        {
            if (input.dynamic_mesh)
            {
                auto mesh = RenderingMeshUploadData();
                const Result result =
                    resource_uploader.upload_dynamic_mesh(input.dynamic_mesh, resource_tracker, mesh);
                if (result)
                    meshes.push_back(mesh);
            }
        }
        else if (input.type == RenderingDrawCommandInputType::STATIC_RUNTIME)
        {
            if (input.runtime_mesh)
            {
                auto mesh = RenderingMeshUploadData();
                const Result result = resource_uploader.upload_static_runtime_mesh(
                    input.handle,
                    *input.runtime_mesh,
                    resource_tracker,
                    mesh);
                if (result)
                    meshes.push_back(mesh);
            }
        }
        else if (input.handle.is_valid())
        {
            const Result result =
                resource_uploader.upload_model_meshes(input.handle, resource_tracker, meshes);
            if (!result)
                return result;
        }

        if (meshes.empty())
        {
            const Result result = resource_uploader.upload_fallback_mesh(resource_tracker, meshes);
            if (!result)
                return result;
        }

        auto material = RenderingMaterialUploadData();
        auto result = resource_uploader.upload_material(input.material, resource_tracker, material);
        if (!result)
            return result;

        const std::string instance_key = detail::make_draw_instance_key(input);
        auto instances = input.instances;
        if (instances.empty())
        {
            instances.push_back(
                RenderingDrawInstanceData {
                    .model_matrix = input.model_matrix,
                    .normal_matrix = input.normal_matrix,
                });
        }

        const auto object_shader_data = ObjectShaderData {
            .model = instances.front().model_matrix,
            .normal_matrix = instances.front().normal_matrix,
        };
        const GraphicsResourceBinding object_uniform_buffer =
            resource_uploader.upload_uniform_buffer(
                resource_tracker,
                BINDING_OBJECT_DATA,
                "Object Shader Data",
                instance_key + "/Object",
                frame_index,
                &object_shader_data,
                static_cast<uint64>(sizeof(object_shader_data)));
        const GraphicsResourceBinding material_uniform_buffer =
            resource_uploader.upload_uniform_buffer(
                resource_tracker,
                BINDING_MATERIAL_DATA,
                "Material Shader Data",
                instance_key + "/Material",
                frame_index,
                material.uniform_values.data(),
                static_cast<uint64>(material.uniform_values.size())
                    * static_cast<uint64>(sizeof(Vec4)));
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

        const auto uniform_bindings = std::vector<GraphicsResourceBinding> {
            frame_uniform_buffers[0],
            frame_uniform_buffers[1],
            object_uniform_buffer,
            material_uniform_buffer,
            frame_uniform_buffers[2],
        };

        for (const auto& mesh : meshes)
        {
            out_draw_commands.push_back(
                GraphicsIndexedDrawCommand {
                    .pipeline = material.pipeline,
                    .index_buffer = mesh.index_buffer,
                    .index_type = GraphicsIndexType::UINT32,
                    .vertex_buffers = {GraphicsResourceBinding {
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
