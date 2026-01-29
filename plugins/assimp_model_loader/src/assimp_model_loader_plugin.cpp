#include "assimp_model_loader_plugin.h"
#include "tbx/app/application.h"
#include "tbx/assets/messages.h"
#include "tbx/files/filesystem.h"
#include "tbx/graphics/material.h"
#include "tbx/graphics/mesh.h"
#include "tbx/graphics/model.h"
#include "tbx/graphics/vertex.h"
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <string>
#include <vector>

namespace tbx::plugins
{
    // Formats a readable failure message for load errors.
    static std::string build_load_failure_message(
        const std::filesystem::path& path,
        const char* reason)
    {
        // Build a message that includes the path and optional Assimp error string.
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

    // Converts an Assimp matrix to the engine Mat4 type.
    static Mat4 to_mat4(const aiMatrix4x4& matrix)
    {
        return Mat4(
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

    // Converts an Assimp vector to a 2D engine vector.
    static Vec2 to_vec2(const aiVector3D& vector)
    {
        return Vec2(vector.x, vector.y);
    }

    // Converts an Assimp vector to a 3D engine vector.
    static Vec3 to_vec3(const aiVector3D& vector)
    {
        return Vec3(vector.x, vector.y, vector.z);
    }

    // Converts an Assimp color to the engine RGBA color type.
    static RgbaColor to_color(const aiColor4D& color)
    {
        return RgbaColor(color.r, color.g, color.b, color.a);
    }

    // Queries a diffuse color from an Assimp material, falling back to white.
    static RgbaColor get_material_diffuse_color(const aiMaterial& material)
    {
        aiColor4D diffuse = {};
        if (aiGetMaterialColor(&material, AI_MATKEY_COLOR_DIFFUSE, &diffuse) == AI_SUCCESS)
        {
            return to_color(diffuse);
        }

        return RgbaColor(1.0f, 1.0f, 1.0f, 1.0f);
    }

    // Defines the default vertex layout used when creating mesh buffers.
    static VertexBufferLayout get_default_mesh_layout()
    {
        return {{
            Vec3(0.0f),
            RgbaColor(),
            Vec3(0.0f),
            Vec2(0.0f),
        }};
    }

    // Recursively traverses the Assimp node hierarchy to build model parts.
    static void append_parts_from_node(
        const aiNode& node,
        const Mat4& parent_transform,
        const std::vector<uint32>& mesh_material_indices,
        std::vector<ModelPart>& parts,
        uint32 parent_index,
        bool has_parent)
    {
        // Compose the local transform with the accumulated parent transform.
        const Mat4 local_transform = parent_transform * to_mat4(node.mTransformation);
        uint32 first_part_index = 0U;
        bool has_first_part = false;

        for (uint32 mesh_offset = 0; mesh_offset < node.mNumMeshes; ++mesh_offset)
        {
            // Use the mesh index referenced by the node.
            const uint32 mesh_index = static_cast<uint32>(node.mMeshes[mesh_offset]);
            // Create a model part referencing the mesh/material and local transform.
            ModelPart part = {};
            part.transform = local_transform;
            part.mesh_index = mesh_index;
            // Clamp material index to available materials.
            const uint32 material_index = (mesh_index < mesh_material_indices.size())
                                              ? mesh_material_indices[mesh_index]
                                              : 0U;
            part.material_index = material_index;
            parts.push_back(part);

            // Track the newly created part index for hierarchy wiring.
            const uint32 part_index = static_cast<uint32>(parts.size() - 1U);
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

        // Determine which part should own the children for the next recursion level.
        const bool next_has_parent = has_parent || has_first_part;
        const uint32 next_parent_index = has_parent ? parent_index : first_part_index;

        // Recurse through child nodes to build nested parts.
        for (uint32 child_index = 0; child_index < node.mNumChildren; ++child_index)
        {
            append_parts_from_node(
                *node.mChildren[child_index],
                local_transform,
                mesh_material_indices,
                parts,
                next_parent_index,
                next_has_parent);
        }
    }

    // Captures a filesystem reference for asset resolution.
    void AssimpModelLoaderPlugin::on_attach(Application& host)
    {
        _filesystem = &host.get_filesystem();
    }

    // Clears cached references on detach.
    void AssimpModelLoaderPlugin::on_detach()
    {
        _filesystem = nullptr;
    }

    // Handles incoming messages for model load requests.
    void AssimpModelLoaderPlugin::on_recieve_message(Message& msg)
    {
        auto* request = handle_message<LoadModelRequest>(msg);
        if (!request)
        {
            return;
        }

        on_load_model_request(*request);
    }

    // Performs the model import and conversion work.
    void AssimpModelLoaderPlugin::on_load_model_request(LoadModelRequest& request)
    {
        // Validate request payload before attempting to import.
        auto* asset = request.asset;
        if (!asset)
        {
            request.state = MessageState::Error;
            request.result.flag_failure("Assimp model loader: missing model asset wrapper.");
            return;
        }

        // Honor cancellation requests before doing any work.
        if (request.cancellation_token && request.cancellation_token.is_cancelled())
        {
            request.state = MessageState::Cancelled;
            request.result.flag_failure("Assimp model loader cancelled.");
            return;
        }

        if (!_filesystem)
        {
            request.state = MessageState::Error;
            request.result.flag_failure("Assimp model loader: filesystem unavailable.");
            return;
        }

        // Resolve the file path against the assets directory.
        const std::filesystem::path resolved = resolve_asset_path(request.path);
        Assimp::Importer importer;
        // Configure Assimp post-processing for engine-friendly meshes.
        const unsigned int flags = aiProcess_Triangulate | aiProcess_GenNormals
                                   | aiProcess_JoinIdenticalVertices | aiProcess_FlipUVs;
        // Load the scene with Assimp.
        const aiScene* scene = importer.ReadFile(resolved.string(), flags);
        if (!scene || !scene->HasMeshes())
        {
            request.state = MessageState::Error;
            request.result.flag_failure(
                build_load_failure_message(resolved, importer.GetErrorString()));
            return;
        }

        // Build materials from Assimp material data.
        std::vector<Material> materials;
        materials.reserve(scene->mNumMaterials);
        for (uint32 material_index = 0; material_index < scene->mNumMaterials; ++material_index)
        {
            const aiMaterial* source_material = scene->mMaterials[material_index];
            Material material = {};
            if (source_material)
            {
                material.color = get_material_diffuse_color(*source_material);
            }
            materials.push_back(material);
        }
        // Ensure at least one material exists for mesh references.
        if (materials.empty())
        {
            materials.push_back(Material());
        }

        // Convert Assimp meshes into engine Mesh instances.
        std::vector<Mesh> meshes;
        meshes.reserve(scene->mNumMeshes);
        // Track material indices per mesh for model part creation.
        std::vector<uint32> mesh_material_indices;
        mesh_material_indices.reserve(scene->mNumMeshes);
        const VertexBufferLayout layout = get_default_mesh_layout();

        // Convert each mesh in the scene.
        for (uint32 mesh_index = 0; mesh_index < scene->mNumMeshes; ++mesh_index)
        {
            // Grab the Assimp mesh pointer for conversion.
            const aiMesh* mesh = scene->mMeshes[mesh_index];
            if (!mesh)
            {
                // Keep indices aligned even if a mesh is missing.
                mesh_material_indices.push_back(0U);
                meshes.push_back(Mesh());
                continue;
            }

            // Clamp material index to available materials.
            const uint32 material_index = (mesh->mMaterialIndex < materials.size())
                                              ? static_cast<uint32>(mesh->mMaterialIndex)
                                              : 0U;
            mesh_material_indices.push_back(material_index);
            const RgbaColor material_color = materials.at(material_index).color;

            // Convert vertices for this mesh.
            std::vector<Vertex> vertices;
            vertices.reserve(mesh->mNumVertices);

            // Populate vertex attributes from Assimp buffers.
            for (uint32 vertex_index = 0; vertex_index < mesh->mNumVertices; ++vertex_index)
            {
                // Fill a single vertex from the Assimp vertex data.
                Vertex vertex = {};
                vertex.position = to_vec3(mesh->mVertices[vertex_index]);
                if (mesh->HasNormals())
                {
                    vertex.normal = to_vec3(mesh->mNormals[vertex_index]);
                }
                if (mesh->HasTextureCoords(0))
                {
                    vertex.uv = to_vec2(mesh->mTextureCoords[0][vertex_index]);
                }
                if (mesh->HasVertexColors(0))
                {
                    vertex.color = to_color(mesh->mColors[0][vertex_index]);
                }
                else
                {
                    vertex.color = material_color;
                }
                vertices.push_back(vertex);
            }

            // Build index buffer from mesh faces.
            IndexBuffer indices;
            indices.reserve(mesh->mNumFaces * 3U);
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
            const VertexBuffer vertex_buffer(vertices, layout);
            meshes.emplace_back(vertex_buffer, indices);
        }

        // Build model parts from the node hierarchy.
        std::vector<ModelPart> parts;
        parts.reserve(meshes.size());
        // Walk the node tree only when we have mesh references.
        if (scene->mRootNode && !mesh_material_indices.empty())
        {
            append_parts_from_node(
                *scene->mRootNode,
                Mat4(1.0f),
                mesh_material_indices,
                parts,
                0U,
                false);
        }

        // Fallback to a single part if no hierarchy was built.
        if (parts.empty())
        {
            // Create a model part referencing the mesh/material and local transform.
            ModelPart part = {};
            part.mesh_index = 0U;
            part.material_index = 0U;
            parts.push_back(part);
        }

        // Preserve the existing asset id when replacing the payload.
        const Uuid existing_id = asset->id;
        // Assemble the final model payload.
        Model model = {};
        model.meshes = std::move(meshes);
        model.materials = std::move(materials);
        model.parts = std::move(parts);
        model.id = existing_id;
        *asset = std::move(model);

        // Mark the request as handled on success.
        request.state = MessageState::Handled;
    }

    // Resolves relative asset paths against the filesystem assets directory.
    std::filesystem::path AssimpModelLoaderPlugin::resolve_asset_path(
        const std::filesystem::path& path) const
    {
        // Only prepend the assets directory for relative paths when available.
        if (path.is_absolute() || !_filesystem)
        {
            return path;
        }

        return _filesystem->get_assets_directory() / path;
    }
}
