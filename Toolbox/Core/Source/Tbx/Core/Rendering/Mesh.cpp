#include "Tbx/Core/PCH.h"
#include "Tbx/Core/Rendering/Mesh.h"

namespace Tbx
{
    Mesh::Mesh(const std::initializer_list<Vertex>& vertices, const std::initializer_list<uint32>& indices)
    {
        _vertexBuffer = VertexVectorToBuffer(vertices);
        _indexBuffer = indices;
    }

    Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32>& indices)
    {
        _vertexBuffer = VertexVectorToBuffer(vertices);
        _indexBuffer = indices;
    }

    Mesh Mesh::MakeTriangle(const Color& color)
    {
        const auto& meshVerts = 
        {
            Tbx::Vertex(Vector3(-0.5f, -0.5f, 0.0f), color),
            Tbx::Vertex(Vector3(0.5f, -0.5f, 0.0f), color),
            Tbx::Vertex(Vector3(0.0f, 0.5f, 0.0f), color)
        };
        const std::vector<uint32>& meshIndices = { 0, 1, 2 };
        const auto& mesh = Mesh(meshVerts, meshIndices);
        return mesh;
    }

    VertexBuffer Mesh::VertexVectorToBuffer(const std::vector<Vertex>& vertices) const
    {
        auto numberOfVertices = vertices.size();
        std::vector<float> meshPoints(numberOfVertices * 12);
        int positionToPlace = 0;

        for (const auto& vertex : vertices)
        {
            const auto& position = vertex.Position;
            meshPoints[positionToPlace] = position.X;
            meshPoints[positionToPlace + 1] = position.Y;
            meshPoints[positionToPlace + 2] = position.Z;

            const auto& color = vertex.Color;
            meshPoints[positionToPlace + 3] = color.R;
            meshPoints[positionToPlace + 4] = color.G;
            meshPoints[positionToPlace + 5] = color.B;
            meshPoints[positionToPlace + 6] = color.A;

            const auto& normal = vertex.Normal;
            meshPoints[positionToPlace + 7] = normal.X;
            meshPoints[positionToPlace + 8] = normal.Y;
            meshPoints[positionToPlace + 9] = normal.Z;

            const auto& textCoord = vertex.TexCoord;
            meshPoints[positionToPlace + 10] = (float)textCoord.X;
            meshPoints[positionToPlace + 11] = (float)textCoord.Y;

            positionToPlace += 12;
        }

        const BufferLayout& bufferLayout =
        {
            { ShaderDataType::Float3, "position" },
            { ShaderDataType::Float4, "color" },
            { ShaderDataType::Float3, "normal" },
            { ShaderDataType::Float2, "textureCoord" },
        };

        return VertexBuffer(meshPoints, bufferLayout);
    }
}