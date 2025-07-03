#include "Tbx/PCH.h"
#include "Tbx/Graphics/Mesh.h"

namespace Tbx
{
    Mesh::Mesh()
    {
        auto quad = MakeQuad();
        _vertices = quad.GetVertices();
        _vertexBuffer = VertexVectorToBuffer(_vertices);
        _indices = quad.GetIndices();
    }

    Mesh::Mesh(const Mesh& mesh)
    {
        _vertices = mesh.GetVertices();
        _vertexBuffer = VertexVectorToBuffer(_vertices);
        _indices = mesh.GetIndices();
    }

    Mesh::Mesh(const std::initializer_list<Vertex>& vertices, const std::initializer_list<uint32>& indices)
    {
        _vertices = vertices;
        _vertexBuffer = VertexVectorToBuffer(vertices);
        _indices = indices;
    }

    Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32>& indices)
    {
        _vertices = vertices;
        _vertexBuffer = VertexVectorToBuffer(vertices);
        _indices = indices;
    }

    const std::vector<Vertex>& Mesh::GetVertices() const
    {
        return _vertices;
    }

    const VertexBuffer& Mesh::GetVertexBuffer() const
    {
        return _vertexBuffer;
    }

    void Mesh::SetVertices(const std::vector<Vertex>& vertices)
    {
        _vertices = vertices;
        _vertexBuffer = VertexVectorToBuffer(vertices);
    }

    const std::vector<uint32>& Mesh::GetIndices() const
    {
        return _indices;
    }

    void Mesh::SetIndices(const std::vector<uint32>& indices)
    {
        _indices = indices;
    }

    Mesh Mesh::MakeTriangle()
    {
        const auto& meshVerts = 
        {
            Tbx::Vertex(Vector3(-0.5f, -0.5f, 0.0f)),
            Tbx::Vertex(Vector3(0.5f, -0.5f, 0.0f)),
            Tbx::Vertex(Vector3(0.0f, 0.5f, 0.0f))
        };
        const std::vector<uint32>& meshIndices = { 0, 1, 2 };
        const auto& mesh = Mesh(meshVerts, meshIndices);
        return mesh;
    }

    Mesh Mesh::MakeQuad()
    {
        const auto& quadMeshVerts =
        {
            Vertex(
                Vector3(-0.5f, -0.5f, 0.0f),    // Position
                Vector3(0.0f, 0.0f, 0.0f),      // Normal
                Vector2I(0, 0),                 // Texture coordinates
                Color(0.0f, 0.0f, 0.0f, 1.0f)), // Color

            Vertex(
                Vector3(0.5f, -0.5f, 0.0f),     // Position
                Vector3(0.0f, 0.0f, 0.0f),      // Normal
                Vector2I(1, 0),                 // Texture coordinates
                Color(0.0f, 0.0f, 0.0f, 1.0f)), // Color

            Vertex(
                Vector3(0.5f, 0.5f, 0.0f),		 // Position
                Vector3(0.0f, 0.0f, 0.0f),      // Normal
                Vector2I(1, 1),                 // Texture coordinates
                Color(0.0f, 0.0f, 0.0f, 1.0f)), // Color

            Vertex(
                Vector3(-0.5f, 0.5f, 0.0f),     // Position
                Vector3(0.0f, 0.0f, 0.0f),      // Normal
                Vector2I(0, 1),                 // Texture coordinates
                Color(0.0f, 0.0f, 0.0f, 1.0f))  // Color
        };
        const std::vector<uint32>& squareMeshIndices = { 0, 1, 2, 2, 3, 0 };
        return Mesh(quadMeshVerts, squareMeshIndices);
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
            { ShaderDataType::Float4, "vertColor" },
            { ShaderDataType::Float3, "normal" },
            { ShaderDataType::Float2, "textureCoord" },
        };

        return VertexBuffer(meshPoints, bufferLayout);
    }
}