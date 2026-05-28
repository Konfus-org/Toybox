#pragma once
#include "tbx/plugins/assimp_model_loader/assimp_model_loader_plugin.h"
#include "tbx/systems/assets/serialization_registry.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/assets/model.h"
#include "tbx/types/assets/material.h"
#include "tbx/types/matrices.h"
#include "tbx/types/vertex.h"
#include "tbx/utils/string_utils.h"
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/types.h>
#include <string>
#include <vector>

namespace assimp_model_loader::internal
{
    static std::string build_load_failure_message(
        const std::filesystem::path& path,
        const char* reason)
    {
        std::string message = "Assimp model loader failed to load model: ";
        message.append(path.string());
        if (reason && *reason)
        {
            message.append(" (reason: ");
            message.append(reason);
            message.append(")");
        }
        return message;
    }

    static tbx::Mat4 to_mat4(const aiMatrix4x4& matrix)
    {
        return tbx::Mat4(
            matrix.a1,
            matrix.b1,
            matrix.c1,
            matrix.d1,
            matrix.a2,
            matrix.b2,
            matrix.c2,
            matrix.d2,
            matrix.a3,
            matrix.b3,
            matrix.c3,
            matrix.d3,
            matrix.a4,
            matrix.b4,
            matrix.c4,
            matrix.d4);
    }

    static tbx::Vec2 to_vec2(const aiVector3D& vector)
    {
        return tbx::Vec2(vector.x, vector.y);
    }

    static tbx::Vec3 to_vec3(const aiVector3D& vector)
    {
        return tbx::Vec3(vector.x, vector.y, vector.z);
    }

    static tbx::Color to_color(const aiColor4D& color)
    {
        return tbx::Color(color.r, color.g, color.b, color.a);
    }

    static float get_default_scale_to_meters_for_path(const std::filesystem::path& path)
    {
        if (const std::string extension = tbx::to_lower(path.extension().string());
            extension == ".fbx")
            return 0.01f;

        return 1.0f;
    }

    static float get_scene_scale_to_meters(
        const aiScene& scene,
        const std::filesystem::path& source_path)
    {
        if (!scene.mMetaData)
        {
            return get_default_scale_to_meters_for_path(source_path);
        }

        ai_real unit_scale = 0.0;
        if (!scene.mMetaData->Get("UnitScaleFactor", unit_scale))
        {
            scene.mMetaData->Get("OriginalUnitScaleFactor", unit_scale);
        }

        if (unit_scale > static_cast<ai_real>(0.0))
        {
            return unit_scale * static_cast<ai_real>(0.01);
        }

        return get_default_scale_to_meters_for_path(source_path);
    }

    static tbx::Color get_material_diffuse_color(const aiMaterial& material)
    {
        aiColor4D diffuse = {};
        if (aiGetMaterialColor(&material, AI_MATKEY_COLOR_DIFFUSE, &diffuse) == AI_SUCCESS)
        {
            return to_color(diffuse);
        }

        return tbx::Color(1.0f, 1.0f, 1.0f, 1.0f);
    }

    static tbx::VertexBufferLayout get_default_mesh_layout()
    {
        return tbx::get_default_vertex_buffer_layout();
    }

    static void append_parts_from_node(
        const aiNode& node,
        const tbx::Mat4& accumulated_transform,
        const std::vector<uint32>& mesh_material_indices,
        std::vector<tbx::ModelPart>& parts,
        const uint32 parent_index,
        const bool has_parent)
    {
        // Accumulate transform-only ancestors until a model part is emitted.
        const tbx::Mat4 local_transform = accumulated_transform * to_mat4(node.mTransformation);
        uint32 first_part_index = 0U;
        bool has_first_part = false;

        for (uint32 mesh_offset = 0; mesh_offset < node.mNumMeshes; ++mesh_offset)
        {
            // Use the mesh index referenced by the node.
            const uint32 mesh_index = node.mMeshes[mesh_offset];
            // Store the transform relative to the nearest emitted parent part.
            tbx::ModelPart part = {};
            part.transform = local_transform;
            part.mesh_index = mesh_index;
            // Clamp material index to available materials.
            const uint32 material_index =
                mesh_index < mesh_material_indices.size() ? mesh_material_indices[mesh_index] : 0U;
            part.material_index = material_index;
            parts.push_back(part);

            // Track the newly created part index for hierarchy wiring.
            uint32 part_index = static_cast<uint32>(parts.size() - 1U);
            // Attach this part as a child of the parent when applicable.
            if (has_parent)
            {
                parts.at(parent_index).children.push_back(part_index);
            }

            if (!has_first_part)
            {
                first_part_index = part_index;
                has_first_part = true;
            }
        }

        // Children become relative to the first emitted part on this node. When the node only
        // contributes transform, keep accumulating until a descendant emits a part.
        const bool next_has_parent = has_parent || has_first_part;
        const uint32 next_parent_index = has_first_part ? first_part_index : parent_index;
        const tbx::Mat4 next_accumulated_transform =
            has_first_part ? tbx::Mat4(1.0f) : local_transform;

        // Recurse through child nodes to build nested parts.
        for (uint32 child_index = 0; child_index < node.mNumChildren; ++child_index)
        {
            append_parts_from_node(
                *node.mChildren[child_index],
                next_accumulated_transform,
                mesh_material_indices,
                parts,
                next_parent_index,
                next_has_parent);
        }
    }

}
