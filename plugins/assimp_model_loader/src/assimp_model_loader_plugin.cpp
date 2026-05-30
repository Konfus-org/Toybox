#include "tbx/plugins/assimp_model_loader/assimp_model_loader_plugin.h"
#include "tbx/systems/assets/serialization_registry.h"
#include "tbx/types/assets/material.h"
#include "tbx/types/assets/model.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/matrices.h"
#include "tbx/types/trig.h"
#include "tbx/types/vertex.h"
#include "tbx/utils/string_utils.h"
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/types.h>

namespace assimp_model_loader
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

    struct ModelPartQueueEntry
    {
        size_t part_index = 0U;
        tbx::Mat4 parent_transform = tbx::Mat4(1.0F);
    };

    static tbx::Vec3 transform_direction(const tbx::Mat3& transform, const tbx::Vec3& direction)
    {
        const tbx::Vec3 transformed = transform * direction;
        const tbx::Vec3 normalized = tbx::normalize_or_zero(transformed);
        return tbx::dot(normalized, normalized) > 0.0F ? normalized : direction;
    }

    static tbx::Vertex transform_vertex(const tbx::Vertex& vertex, const tbx::Mat4& transform)
    {
        tbx::Vertex result = vertex;
        result.position = tbx::Vec3(transform * tbx::Vec4(vertex.position, 1.0F));

        const tbx::Mat3 normal_transform = tbx::inverse_transpose(tbx::Mat3(transform));
        result.normal = transform_direction(normal_transform, vertex.normal);

        const tbx::Vec3 tangent =
            transform_direction(tbx::Mat3(transform), tbx::Vec3(vertex.tangent));
        result.tangent = tbx::Vec4(tangent, vertex.tangent.w);
        return result;
    }

    static tbx::Mesh make_transformed_mesh(
        const tbx::Mesh& source,
        const tbx::Mat4& transform,
        const tbx::VertexBufferLayout& layout)
    {
        const uint32 stride = source.get_vertex_stride_float_count();
        if (stride < 16U || source.vertices.vertices.size() % static_cast<size_t>(stride) != 0U)
            return source;

        auto vertices = std::vector<tbx::Vertex>();
        vertices.reserve(source.vertices.vertices.size() / static_cast<size_t>(stride));
        for (size_t vertex_offset = 0U; vertex_offset < source.vertices.vertices.size();
             vertex_offset += static_cast<size_t>(stride))
        {
            tbx::Vertex vertex = {};
            vertex.position = tbx::Vec3(
                source.vertices.vertices[vertex_offset + 0U],
                source.vertices.vertices[vertex_offset + 1U],
                source.vertices.vertices[vertex_offset + 2U]);
            vertex.color = tbx::Color(
                source.vertices.vertices[vertex_offset + 3U],
                source.vertices.vertices[vertex_offset + 4U],
                source.vertices.vertices[vertex_offset + 5U],
                source.vertices.vertices[vertex_offset + 6U]);
            vertex.normal = tbx::Vec3(
                source.vertices.vertices[vertex_offset + 7U],
                source.vertices.vertices[vertex_offset + 8U],
                source.vertices.vertices[vertex_offset + 9U]);
            vertex.uv = tbx::Vec2(
                source.vertices.vertices[vertex_offset + 10U],
                source.vertices.vertices[vertex_offset + 11U]);
            vertex.tangent = tbx::Vec4(
                source.vertices.vertices[vertex_offset + 12U],
                source.vertices.vertices[vertex_offset + 13U],
                source.vertices.vertices[vertex_offset + 14U],
                source.vertices.vertices[vertex_offset + 15U]);
            vertices.push_back(transform_vertex(vertex, transform));
        }

        return tbx::Mesh(tbx::VertexBuffer(vertices, layout), source.indices);
    }

    static void bake_model_part_transforms(
        const std::vector<tbx::Mesh>& source_meshes,
        std::vector<tbx::ModelPart>& parts,
        const tbx::VertexBufferLayout& layout,
        std::vector<tbx::Mesh>& out_meshes)
    {
        out_meshes.clear();
        if (parts.empty())
        {
            out_meshes = source_meshes;
            return;
        }

        auto has_parent = std::vector<bool>(parts.size(), false);
        for (const auto& part : parts)
            for (const auto child_index : part.children)
                if (child_index < has_parent.size())
                    has_parent[child_index] = true;

        auto queue = std::vector<ModelPartQueueEntry>();
        queue.reserve(parts.size());
        for (size_t part_index = 0U; part_index < parts.size(); ++part_index)
        {
            if (has_parent[part_index])
                continue;

            queue.push_back(
                ModelPartQueueEntry {
                    .part_index = part_index,
                    .parent_transform = tbx::Mat4(1.0F),
                });
        }

        if (queue.empty())
        {
            queue.push_back(
                ModelPartQueueEntry {
                    .part_index = 0U,
                    .parent_transform = tbx::Mat4(1.0F),
                });
        }

        auto visited_parts = std::vector<bool>(parts.size(), false);
        while (!queue.empty())
        {
            const ModelPartQueueEntry current = queue.back();
            queue.pop_back();
            if (current.part_index >= parts.size())
                continue;
            if (visited_parts[current.part_index])
                continue;
            visited_parts[current.part_index] = true;

            auto& part = parts[current.part_index];
            const tbx::Mat4 part_transform = current.parent_transform * part.transform;
            if (part.mesh_index < source_meshes.size())
            {
                const uint32 source_mesh_index = part.mesh_index;
                part.mesh_index = static_cast<uint32>(out_meshes.size());
                out_meshes.push_back(make_transformed_mesh(
                    source_meshes[static_cast<size_t>(source_mesh_index)],
                    part_transform,
                    layout));
            }
            part.transform = tbx::Mat4(1.0F);

            for (const auto child_index : part.children)
            {
                queue.push_back(
                    ModelPartQueueEntry {
                        .part_index = child_index,
                        .parent_transform = part_transform,
                    });
            }
        }
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

    void AssimpModelLoaderPlugin::on_attach(tbx::ServiceProvider& service_provider)
    {
        _serialization_registry = service_provider.get_service<tbx::SerializationRegistry>();
        if (auto serialization_registry = _serialization_registry.lock())
            serialization_registry->register_loader<tbx::Model>(read_model);
    }

    void AssimpModelLoaderPlugin::on_detach(tbx::ServiceProvider&)
    {
        if (auto serialization_registry = _serialization_registry.lock())
            serialization_registry->deregister_loader<tbx::Model>();

        _serialization_registry = {};
    }

    tbx::Result AssimpModelLoaderPlugin::read_model(
        const std::filesystem::path& asset_path,
        const tbx::ModelLoadParameters&,
        const tbx::AssetLoadMetadata&,
        tbx::Model& model)
    {
        auto result = tbx::Result {};
        Assimp::Importer importer;
        // Configure Assimp post-processing for engine-friendly meshes.
        unsigned int flags = aiProcess_Triangulate | aiProcess_GenNormals
                             | aiProcess_CalcTangentSpace | aiProcess_JoinIdenticalVertices
                             | aiProcess_FlipUVs;
        // Load the scene with Assimp.
        const aiScene* scene = importer.ReadFile(asset_path.string(), flags);
        if (!scene || !scene->HasMeshes())
        {
            result.flag_failure(build_load_failure_message(asset_path, importer.GetErrorString()));
            return result;
        }

        // Build materials from Assimp material data.
        std::vector<tbx::Material> materials;
        materials.reserve(scene->mNumMaterials);
        for (uint32 material_index = 0; material_index < scene->mNumMaterials; ++material_index)
        {
            const aiMaterial* source_material = scene->mMaterials[material_index];
            tbx::Material material = {};
            if (source_material)
            {
                material.parameters.set("color", get_material_diffuse_color(*source_material));
            }
            materials.push_back(material);
        }
        // Ensure at least one material exists for mesh references.
        if (materials.empty())
        {
            materials.push_back(tbx::Material());
        }

        // Convert Assimp meshes into engine tbx::Mesh instances.
        std::vector<tbx::Mesh> meshes;
        meshes.reserve(scene->mNumMeshes);
        // Track material indices per mesh for model part creation.
        std::vector<uint32> mesh_material_indices;
        mesh_material_indices.reserve(scene->mNumMeshes);
        tbx::VertexBufferLayout layout = get_default_mesh_layout();

        // Convert each mesh in the scene.
        for (uint32 mesh_index = 0; mesh_index < scene->mNumMeshes; ++mesh_index)
        {
            // Grab the Assimp mesh pointer for conversion.
            const aiMesh* mesh = scene->mMeshes[mesh_index];
            if (!mesh)
            {
                // Keep indices aligned even if a mesh is missing.
                mesh_material_indices.push_back(0U);
                meshes.push_back(tbx::Mesh());
                continue;
            }

            // Clamp material index to available materials.
            uint32 material_index = mesh->mMaterialIndex < materials.size()
                                        ? static_cast<uint32>(mesh->mMaterialIndex)
                                        : 0U;
            mesh_material_indices.push_back(material_index);

            // Convert vertices for this mesh.
            std::vector<tbx::Vertex> vertices;
            vertices.reserve(mesh->mNumVertices);

            // Populate vertex attributes from Assimp buffers.
            for (uint32 vertex_index = 0; vertex_index < mesh->mNumVertices; ++vertex_index)
            {
                // Fill a single vertex from the Assimp vertex data.
                tbx::Vertex vertex = {};
                vertex.position = to_vec3(mesh->mVertices[vertex_index]);
                if (mesh->HasNormals())
                    vertex.normal = to_vec3(mesh->mNormals[vertex_index]);
                if (mesh->HasTextureCoords(0))
                    vertex.uv = to_vec2(mesh->mTextureCoords[0][vertex_index]);
                if (mesh->HasTangentsAndBitangents())
                {
                    const auto tangent = to_vec3(mesh->mTangents[vertex_index]);
                    const auto bitangent = to_vec3(mesh->mBitangents[vertex_index]);
                    auto tangent_handedness = 1.0F;
                    if (tbx::dot(tbx::cross(vertex.normal, tangent), bitangent) < 0.0F)
                        tangent_handedness = -1.0F;
                    vertex.tangent = tbx::Vec4(tangent.x, tangent.y, tangent.z, tangent_handedness);
                }
                if (mesh->HasVertexColors(0))
                    vertex.color = to_color(mesh->mColors[0][vertex_index]);
                else
                    vertex.color = tbx::Color(1.0f, 1.0f, 1.0f, 1.0f);
                vertices.push_back(vertex);
            }

            // Build index buffer from mesh faces.
            tbx::IndexBuffer indices;
            indices.reserve(static_cast<size_t>(mesh->mNumFaces) * static_cast<size_t>(3U));
            // Append all indices from each face.
            for (uint32 face_index = 0; face_index < mesh->mNumFaces; ++face_index)
            {
                const aiFace& face = mesh->mFaces[face_index];
                for (uint32 index_offset = 0; index_offset < face.mNumIndices; ++index_offset)
                {
                    indices.push_back(static_cast<uint32>(face.mIndices[index_offset]));
                }
            }

            // Create the vertex buffer using the engine layout.
            tbx::VertexBuffer vertex_buffer(vertices, layout);
            meshes.emplace_back(vertex_buffer, indices);
        }

        // Build model parts from the node hierarchy.
        std::vector<tbx::ModelPart> parts;
        parts.reserve(meshes.size());
        float scene_scale_to_meters = get_scene_scale_to_meters(*scene, asset_path);
        tbx::Mat4 scene_scale = scene_scale_to_meters == 1.0f
                                    ? tbx::Mat4(1.0f)
                                    : tbx::scale(tbx::Vec3(scene_scale_to_meters));
        if (scene->mRootNode && !mesh_material_indices.empty())
        {
            append_parts_from_node(
                *scene->mRootNode,
                scene_scale,
                mesh_material_indices,
                parts,
                0U,
                false);
        }

        // Fallback to a single part if no hierarchy was built.
        if (parts.empty())
        {
            // Create a model part referencing the mesh/material and local transform.
            tbx::ModelPart part = {};
            part.transform = scene_scale;
            part.mesh_index = 0U;
            part.material_index = 0U;
            parts.push_back(part);
        }

        auto baked_meshes = std::vector<tbx::Mesh>();
        bake_model_part_transforms(meshes, parts, layout, baked_meshes);

        // Assemble the final model payload.
        model.meshes = std::move(baked_meshes);
        model.materials = std::move(materials);
        model.parts = std::move(parts);
        result.flag_success();
        return result;
    }
}
