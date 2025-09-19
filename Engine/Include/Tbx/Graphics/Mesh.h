#pragma once
#include "Tbx/Graphics/Vertex.h"
#include "Tbx/Graphics/Buffers.h"
#include "Tbx/Ids/UID.h"
#include "Tbx/TypeAliases/Int.h"

namespace Tbx
{
    struct Mesh
    {
    public:
        /// <summary>
        /// Defaults to a quad mesh.
        /// </summary>
        EXPORT Mesh();
        EXPORT Mesh(const Mesh& mesh);
        EXPORT Mesh(const std::initializer_list<Vertex>& vertices, const std::initializer_list<uint32>& indices);
        EXPORT Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32>& indices);

        EXPORT const VertexBuffer& GetVertexBuffer() const;
        EXPORT const std::vector<uint32>& GetIndexBuffer() const;

        EXPORT static Mesh MakeTriangle();
        EXPORT static Mesh MakeQuad();

        EXPORT inline static Mesh Quad = Mesh::MakeQuad();
        EXPORT inline static Mesh Triangle = Mesh::MakeTriangle();

    private:
        VertexBuffer VertexVectorToBuffer(const std::vector<Vertex>& vertices) const;

    private:
        std::vector<uint32> _indexBuffer;
        VertexBuffer _vertexBuffer;
    };

    /// <summary>
    /// Essentially a pointer to a mesh.
    /// </summary>
    struct MeshInstance
    {
        Uid InstanceId = Uid::GetNextId();
        Uid MeshId = Uid::Invalid;
    };

}

