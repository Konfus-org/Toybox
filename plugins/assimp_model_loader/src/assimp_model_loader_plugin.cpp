#include "assimp_model_loader_plugin.h"
#include "tbx/assets/messages.h"
#include "tbx/common/string_utils.h"
#include "tbx/graphics/material.h"
#include "tbx/graphics/mesh.h"
#include "tbx/graphics/model.h"
#include "tbx/graphics/vertex.h"
#include "tbx/math/matrices.h"
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/types.h>
#include <cstddef>
#include <string>
#include <vector>

namespace assimp_model_loader
{
    using namespace tbx;
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
    static Color to_color(const aiColor4D& color)
    {
        return Color(color.r, color.g, color.b, color.a);
    }

    // Determines the scale needed to convert imported scene units to meters.
    static float get_default_scale_to_meters_for_path(const std::filesystem::path& path)
    {
        std::string extension = to_lower(path.extension().string());
        if (extension == ".fbx")
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

        ai_real unit_scale = static_cast<ai_real>(0.0);
        if (!scene.mMetaData->Get("UnitScaleFactor", unit_scale))
        {
            scene.mMetaData->Get("OriginalUnitScaleFactor", unit_scale);
        }

        if (unit_scale > static_cast<ai_real>(0.0))
        {
            return static_cast<float>(unit_scale * static_cast<ai_real>(0.01));
        }

        return get_default_scale_to_meters_for_path(source_path);
    }

    // Queries a diffuse color from an Assimp material, falling back to white.
    static Color get_material_diffuse_color(const aiMaterial& material)
    {
        aiColor4D diffuse = {};
        if (aiGetMaterialColor(&material, AI_MATKEY_COLOR_DIFFUSE, &diffuse) == AI_SUCCESS)
        {
            return to_color(diffuse);
        }

        return Color(1.0f, 1.0f, 1.0f, 1.0f);
    }

    // Defines the default vertex layout used when creating mesh buffers.
    static VertexBufferLayout get_default_mesh_layout()
    {
        return {{
            Vec3(0.0f),
            Color(),
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
        Mat4 local_transform = parent_transform * to_mat4(node.mTransformation);
        uint32 first_part_index = 0U;
        bool has_first_part = false;

        for (uint32 mesh_offset = 0; mesh_offset < node.mNumMeshes; ++mesh_offset)
        {
            // Use the mesh index referenced by the node.
            uint32 mesh_index = static_cast<uint32>(node.mMeshes[mesh_offset]);
            // Create a model part referencing the mesh/material and local transform.
            ModelPart part = {};
            part.transform = local_transform;
            part.mesh_index = mesh_index;
            // Clamp material index to available materials.
            uint32 material_index = (mesh_index < mesh_material_indices.size())
                                        ? mesh_material_indices[mesh_index]
                                        : 0U;
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

        // Determine which part should own the children for the next recursion level.
        bool next_has_parent = has_parent || has_first_part;
        uint32 next_parent_index = has_parent ? parent_index : first_part_index;

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
    void AssimpModelLoaderPlugin::on_attach(IPluginHost&) {}

    // Clears cached references on detach.
    void AssimpModelLoaderPlugin::on_detach() {}

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
            request.state = MessageState::ERROR;
            request.result.flag_failure("Assimp model loader: missing model asset wrapper.");
            return;
        }

        // Honor cancellation requests before doing any work.
        if (request.cancellation_token && request.cancellation_token.is_cancelled())
        {
            request.state = MessageState::CANCELLED;
            request.result.flag_failure("Assimp model loader cancelled.");
            return;
        }

        Assimp::Importer importer;
        // Configure Assimp post-processing for engine-friendly meshes.
        unsigned int flags = aiProcess_Triangulate | aiProcess_GenNormals
                             | aiProcess_JoinIdenticalVertices | aiProcess_FlipUVs;
        // Load the scene with Assimp.
        const aiScene* scene = importer.ReadFile(request.path.string(), flags);
        if (!scene || !scene->HasMeshes())
        {
            request.state = MessageState::ERROR;
            request.result.flag_failure(
                build_load_failure_message(request.path, importer.GetErrorString()));
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
                material.parameters.set("color", get_material_diffuse_color(*source_material));
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
        VertexBufferLayout layout = get_default_mesh_layout();

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
            uint32 material_index = (mesh->mMaterialIndex < materials.size())
                                        ? static_cast<uint32>(mesh->mMaterialIndex)
                                        : 0U;
            mesh_material_indices.push_back(material_index);

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
                    vertex.normal = to_vec3(mesh->mNormals[vertex_index]);
                if (mesh->HasTextureCoords(0))
                    vertex.uv = to_vec2(mesh->mTextureCoords[0][vertex_index]);
                if (mesh->HasVertexColors(0))
                    vertex.color = to_color(mesh->mColors[0][vertex_index]);
                else
                    vertex.color = Color(1.0f, 1.0f, 1.0f, 1.0f);
                vertices.push_back(vertex);
            }

            // Build index buffer from mesh faces.
            IndexBuffer indices;
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
            VertexBuffer vertex_buffer(vertices, layout);
            meshes.emplace_back(vertex_buffer, indices);
        }

        // Build model parts from the node hierarchy.
        std::vector<ModelPart> parts;
        parts.reserve(meshes.size());
        float scene_scale_to_meters = get_scene_scale_to_meters(*scene, request.path);
        Mat4 scene_scale =
            (scene_scale_to_meters == 1.0f) ? Mat4(1.0f) : scale(Vec3(scene_scale_to_meters));
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
            ModelPart part = {};
            part.transform = scene_scale;
            part.mesh_index = 0U;
            part.material_index = 0U;
            parts.push_back(part);
        }

        // Assemble the final model payload.
        Model model = {};
        model.meshes = std::move(meshes);
        model.materials = std::move(materials);
        model.parts = std::move(parts);
        *asset = std::move(model);

        // Mark the request as handled on success.
        request.state = MessageState::HANDLED;
    }
}
