#include "tbx/gpu/model.h"

#include <assimp/config.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>

namespace tbx
{
    Result<Model> gpu_deserialize_model(const std::filesystem::path& path)
    {
        auto importer = Assimp::Importer();
        // Lines/points must go: a 2-index face in the triangle list shifts every vertex after
        // it and shreds the mesh. GlobalScale honors the file's unit (FBX cm etc).
        importer.SetPropertyInteger(
            AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_LINE | aiPrimitiveType_POINT);
        const aiScene* scene = importer.ReadFile(
            path.string(),
            aiProcess_Triangulate | aiProcess_SortByPType | aiProcess_GenSmoothNormals
                | aiProcess_JoinIdenticalVertices | aiProcess_PreTransformVertices
                | aiProcess_GlobalScale);
        if (!scene || !scene->HasMeshes())
            return fail(
                "could not import model '{}': {}", path.string(), importer.GetErrorString());

        // Every mesh merges into one interleaved position+normal+uv triangle list.
        auto model = Model {};
        for (unsigned mesh_index = 0; mesh_index < scene->mNumMeshes; ++mesh_index)
        {
            const aiMesh* mesh = scene->mMeshes[mesh_index];
            if ((mesh->mPrimitiveTypes & aiPrimitiveType_TRIANGLE) == 0)
                continue;
            for (unsigned face_index = 0; face_index < mesh->mNumFaces; ++face_index)
            {
                const aiFace& face = mesh->mFaces[face_index];
                if (face.mNumIndices != 3)
                    continue;
                for (unsigned corner = 0; corner < face.mNumIndices; ++corner)
                {
                    const unsigned vertex = face.mIndices[corner];
                    const aiVector3D position = mesh->mVertices[vertex];
                    const aiVector3D normal =
                        mesh->HasNormals() ? mesh->mNormals[vertex] : aiVector3D(0, 1, 0);
                    const aiVector3D uv = mesh->HasTextureCoords(0)
                        ? mesh->mTextureCoords[0][vertex]
                        : aiVector3D(0, 0, 0);
                    for (const float value :
                         {position.x, position.y, position.z, normal.x, normal.y, normal.z,
                          uv.x, uv.y})
                        model.vertices.push_back(value);
                }
            }
        }
        return model;
    }
}
