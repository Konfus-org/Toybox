#pragma once
#include "Tbx/Graphics/Vertex.h"
#include "Tbx/Graphics/Buffers.h"
#include "Tbx/Ids/UsesUid.h"
#include "Tbx/Math/Int.h"

namespace Tbx
{
    struct EXPORT Mesh : public UsesUid
    {
    public:
        /// <summary>
        /// Defaults to a quad mesh.
        /// </summary>
        Mesh();
        Mesh(const Mesh& mesh);
        Mesh(const std::initializer_list<Vertex>& vertices, const std::initializer_list<uint32>& indices);
        Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32>& indices);

        const VertexBuffer& GetVertexBuffer() const;
        const std::vector<uint32>& GetIndexBuffer() const;

        static Mesh MakeTriangle();
        static Mesh MakeQuad();

        static Mesh Quad;
        static Mesh Triangle;

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

