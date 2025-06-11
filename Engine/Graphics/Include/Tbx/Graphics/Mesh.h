#pragma once
#include "Tbx/Graphics/Buffers.h"
#include "Tbx/Graphics/Vertex.h"
#include <Tbx/Math/Int.h>

namespace Tbx
{
    struct Mesh
    {
    public:
        /// <summary>
        /// Defaults to a quad mesh.
        /// </summary>
        EXPORT Mesh()
        {
            auto quad = MakeQuad();
            _vertexBuffer = quad.GetVertices();
            _indexBuffer = quad.GetIndices();
        }

        EXPORT Mesh(const Mesh& mesh);
        EXPORT Mesh(const std::initializer_list<Vertex>& vertices, const std::initializer_list<uint32>& indices);
        EXPORT Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32>& indices);
        EXPORT Mesh(const VertexBuffer& vertices, const IndexBuffer& indices)
            : _vertexBuffer(vertices), _indexBuffer(indices) {}

        EXPORT const VertexBuffer& GetVertices() const { return _vertexBuffer; }
        EXPORT const IndexBuffer& GetIndices() const { return _indexBuffer; }

        EXPORT void SetVertices(const VertexBuffer& vertices) { _vertexBuffer = vertices; }
        EXPORT void SetIndices(const IndexBuffer& indices) { _indexBuffer = indices; }

        EXPORT static Mesh MakeTriangle();
        EXPORT static Mesh MakeQuad();

    private:
        VertexBuffer VertexVectorToBuffer(const std::vector<Vertex>& vertices) const;

        VertexBuffer _vertexBuffer;  
        IndexBuffer _indexBuffer;
    };

    namespace Primitives
    {
        EXPORT inline const Mesh& Quad = Mesh::MakeQuad();
        EXPORT inline const Mesh& Triangle = Mesh::MakeTriangle();
    }
}

