#include "Tbx/PCH.h"
#include "Tbx/Graphics/Mesh.h"

namespace Tbx
{
    Mesh Mesh::Quad = MakeQuad();
    Mesh Mesh::Triangle = MakeTriangle();

    Mesh MakeTriangle()
    {
        const auto& meshVerts = 
        {
            Tbx::Vertex(Vector3(-0.5f, -0.5f, 0.0f)),
            Tbx::Vertex(Vector3(0.5f, -0.5f, 0.0f)),
            Tbx::Vertex(Vector3(0.0f, 0.5f, 0.0f))
        };
        const std::vector<uint32>& meshIndices = { 0, 1, 2 };
        const auto& mesh = Mesh(VertexBuffer(meshVerts), meshIndices);
        return mesh;
    }

    Mesh MakeQuad()
    {
        const std::vector<Vertex> quadMeshVerts =
        {
            Vertex(
                Vector3(-0.5f, -0.5f, 0.0f),        // Position
                Vector3(0.0f, 0.0f, 0.0f),          // Normal
                Vector2(0, 0),                      // Texture coordinates
                RgbaColor(0.0f, 0.0f, 0.0f, 1.0f)), // Color

            Vertex(
                Vector3(0.5f, -0.5f, 0.0f),         // Position
                Vector3(0.0f, 0.0f, 0.0f),          // Normal
                Vector2(1, 0),                      // Texture coordinates
                RgbaColor(0.0f, 0.0f, 0.0f, 1.0f)), // Color

            Vertex(
                Vector3(0.5f, 0.5f, 0.0f),		    // Position
                Vector3(0.0f, 0.0f, 0.0f),          // Normal
                Vector2(1, 1),                      // Texture coordinates
                RgbaColor(0.0f, 0.0f, 0.0f, 1.0f)), // Color

            Vertex(
                Vector3(-0.5f, 0.5f, 0.0f),        // Position
                Vector3(0.0f, 0.0f, 0.0f),         // Normal
                Vector2(0, 1),                     // Texture coordinates
                RgbaColor(0.0f, 0.0f, 0.0f, 1.0f)) // Color
        };
        const std::vector<uint32> squareMeshIndices = { 0, 1, 2, 2, 3, 0 };
        return Mesh(VertexBuffer(quadMeshVerts), squareMeshIndices);
    }
}