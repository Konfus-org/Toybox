#pragma once
#include "Tbx/Graphics/Vertex.h"
#include "Tbx/Graphics/Buffers.h"
#include "Tbx/Ids/UsesUID.h"
#include "Tbx/TypeAliases/Int.h"

namespace Tbx
{
    struct Mesh : public UsesUid
    {
    public:
        /// <summary>
        /// Defaults to a quad mesh.
        /// </summary>
        EXPORT Mesh();
        EXPORT Mesh(const Mesh& mesh);
        EXPORT Mesh(const std::initializer_list<Vertex>& vertices, const std::initializer_list<uint32>& indices);
        EXPORT Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32>& indices);

        EXPORT const std::vector<Vertex>& GetVertices() const;
        EXPORT const VertexBuffer& GetVertexBuffer() const;
        EXPORT void SetVertices(const std::vector<Vertex>& vertices);

        EXPORT const std::vector<uint32>& GetIndices() const;
        EXPORT void SetIndices(const std::vector<uint32>& indices);

        EXPORT static Mesh MakeTriangle();
        EXPORT static Mesh MakeQuad();

    private:
        VertexBuffer VertexVectorToBuffer(const std::vector<Vertex>& vertices) const;

        std::vector<uint32> _indices;
        std::vector<Vertex> _vertices;
        VertexBuffer _vertexBuffer;
    };

    namespace Primitives
    {
        EXPORT inline const Mesh& Quad = Mesh::MakeQuad();
        EXPORT inline const Mesh& Triangle = Mesh::MakeTriangle();
    }
}

