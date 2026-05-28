#include "tbx/plugins/assimp_model_loader/assimp_model_loader_plugin.h"
#include "internal/assimp_model_loader_plugin_internal.h"
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
namespace assimp_model_loader
{
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
            result.flag_failure(
                internal::build_load_failure_message(asset_path, importer.GetErrorString()));
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
                material.parameters.set(
                    "color",
                    internal::get_material_diffuse_color(*source_material));
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
        tbx::VertexBufferLayout layout = internal::get_default_mesh_layout();

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
                vertex.position = internal::to_vec3(mesh->mVertices[vertex_index]);
                if (mesh->HasNormals())
                    vertex.normal = internal::to_vec3(mesh->mNormals[vertex_index]);
                if (mesh->HasTextureCoords(0))
                    vertex.uv = internal::to_vec2(mesh->mTextureCoords[0][vertex_index]);
                if (mesh->HasTangentsAndBitangents())
                {
                    const auto tangent = internal::to_vec3(mesh->mTangents[vertex_index]);
                    const auto bitangent = internal::to_vec3(mesh->mBitangents[vertex_index]);
                    auto tangent_handedness = 1.0F;
                    if (tbx::dot(tbx::cross(vertex.normal, tangent), bitangent) < 0.0F)
                        tangent_handedness = -1.0F;
                    vertex.tangent = tbx::Vec4(tangent.x, tangent.y, tangent.z, tangent_handedness);
                }
                if (mesh->HasVertexColors(0))
                    vertex.color = internal::to_color(mesh->mColors[0][vertex_index]);
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
        float scene_scale_to_meters = internal::get_scene_scale_to_meters(*scene, asset_path);
        tbx::Mat4 scene_scale = scene_scale_to_meters == 1.0f
                                    ? tbx::Mat4(1.0f)
                                    : tbx::scale(tbx::Vec3(scene_scale_to_meters));
        if (scene->mRootNode && !mesh_material_indices.empty())
        {
            internal::append_parts_from_node(
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

        // Assemble the final model payload.
        model.meshes = std::move(meshes);
        model.materials = std::move(materials);
        model.parts = std::move(parts);
        result.flag_success();
        return result;
    }
}
