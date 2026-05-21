#pragma once
#include "tbx/systems/graphics/draw_command_factory.h"
#include "tbx/systems/graphics/shader_bindings.h"
#include <array>
#include <string>
#include <unordered_map>
#include <vector>

namespace tbx::internal
{
    static std::string make_draw_instance_key(const RenderingDrawBatchInput& input)
    {
        if (!input.debug_name.empty())
            return input.debug_name;

        return std::string("Toybox/Draw/") + std::to_string(input.batch_key);
    }

    static Result resolve_material_upload(
        const uint64 material_key,
        const MaterialInstance& material_instance,
        ResourceUploader& resource_uploader,
        RenderingResourceTracker& resource_tracker,
        std::unordered_map<uint64, RenderingMaterialUploadData>& material_uploads,
        RenderingMaterialUploadData& out_material)
    {
        if (const auto cached_material = material_uploads.find(material_key);
            cached_material != material_uploads.end())
        {
            out_material = cached_material->second;
            return {};
        }

        auto result =
            resource_uploader.upload_material(material_instance, resource_tracker, out_material);
        if (!result)
            return result;

        material_uploads[material_key] = out_material;
        return {};
    }

    static GraphicsResourceBinding upload_object_uniform_buffer(
        const uint64 frame_index,
        const std::string& instance_key,
        const ObjectShaderData& object_shader_data,
        ResourceUploader& resource_uploader,
        RenderingResourceTracker& resource_tracker)
    {
        return resource_uploader.upload_uniform_buffer(
            resource_tracker,
            BINDING_OBJECT_DATA,
            "Object Shader Data",
            instance_key + "/Object",
            frame_index,
            &object_shader_data,
            static_cast<uint64>(sizeof(object_shader_data)));
    }

    static GraphicsResourceBinding upload_material_uniform_buffer(
        const uint64 frame_index,
        const uint64 material_key,
        const RenderingMaterialUploadData& material,
        ResourceUploader& resource_uploader,
        RenderingResourceTracker& resource_tracker,
        std::unordered_map<uint64, GraphicsResourceBinding>& material_uniform_buffers)
    {
        if (const auto cached_uniform = material_uniform_buffers.find(material_key);
            cached_uniform != material_uniform_buffers.end())
        {
            return cached_uniform->second;
        }

        auto material_uniform_buffer = resource_uploader.upload_uniform_buffer(
            resource_tracker,
            BINDING_MATERIAL_DATA,
            "Material Shader Data",
            std::string("Toybox/Material/") + std::to_string(material_key),
            frame_index,
            material.uniform_values.data(),
            static_cast<uint64>(material.uniform_values.size())
                * static_cast<uint64>(sizeof(Vec4)));
        material_uniform_buffers[material_key] = material_uniform_buffer;
        return material_uniform_buffer;
    }

    static std::vector<GraphicsResourceBinding> make_draw_uniform_bindings(
        const std::array<GraphicsResourceBinding, 3U>& frame_uniform_buffers,
        const GraphicsResourceBinding& object_uniform_buffer,
        const GraphicsResourceBinding& material_uniform_buffer)
    {
        return {
            frame_uniform_buffers[0],
            frame_uniform_buffers[1],
            object_uniform_buffer,
            material_uniform_buffer,
            frame_uniform_buffers[2],
        };
    }
}
