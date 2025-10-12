#include "Tbx/PCH.h"
#include "Tbx/Graphics/Mesh.h"

namespace Tbx
{
    Mesh Mesh::Quad = MakeQuad();
    Mesh Mesh::Triangle = MakeTriangle();

    Mesh::Mesh()
    {
        const auto& quad = Quad;
        Vertices = quad.Vertices;
        Indices = quad.Indices;
    }

    Mesh::Mesh(const VertexBuffer& vertBuff, const IndexBuffer& indexBuff)
        : Vertices(vertBuff), Indices(indexBuff)
    {
    }

    Mesh MakeTriangle()
    {
        const auto& triangleMeshVerts = 
        {
            Vertex
            {
                Vector3(-0.5f, -0.5f, 0.0f),        // Position
                Vector3(0.0f, 0.0f, 0.0f),          // Normal
                Vector2(0, 0),                      // Texture coordinates
                RgbaColor(0.0f, 0.0f, 0.0f, 1.0f)   // Color
            },
            Vertex
            {
                Vector3(0.5f, -0.5f, 0.0f),         // Position
                Vector3(0.0f, 0.0f, 0.0f),          // Normal
                Vector2(0, 0),                      // Texture coordinates
                RgbaColor(0.0f, 0.0f, 0.0f, 1.0f)   // Color
            },
            Vertex
            {
                Vector3(0.0f, 0.5f, 0.0f),          // Position
                Vector3(0.0f, 0.0f, 0.0f),          // Normal
                Vector2(0, 0),                      // Texture coordinates
                RgbaColor(0.0f, 0.0f, 0.0f, 1.0f)   // Color
            }
        };

        const IndexBuffer indexBuffer = { 0, 1, 2 };
        const VertexBuffer vertexBuffer =
        {
            triangleMeshVerts,
            {{
                // Pos
                Vector3(),
                // Vert Color
                RgbaColor(),
                // Normal
                Vector3(),
                // Tex Coord
                Vector2(),

            }}
        };

        return { vertexBuffer, indexBuffer };
    }

    Mesh MakeQuad()
    {
        const std::vector<Vertex> quadMeshVerts =
        {
            Vertex
            {
                Vector3(-0.5f, -0.5f, 0.0f),        // Position
                Vector3(0.0f, 0.0f, 0.0f),          // Normal
                Vector2(0, 0),                      // Texture coordinates
                RgbaColor(0.0f, 0.0f, 0.0f, 1.0f)   // Color
            },
            Vertex
            {
                Vector3(0.5f, -0.5f, 0.0f),         // Position
                Vector3(0.0f, 0.0f, 0.0f),          // Normal
                Vector2(1, 0),                      // Texture coordinates
                RgbaColor(0.0f, 0.0f, 0.0f, 1.0f)   // Color
            },
            Vertex
            {
                Vector3(0.5f, 0.5f, 0.0f),		    // Position
                Vector3(0.0f, 0.0f, 0.0f),          // Normal
                Vector2(1, 1),                      // Texture coordinates
                RgbaColor(0.0f, 0.0f, 0.0f, 1.0f)   // Color
            },
            Vertex
            {
                Vector3(-0.5f, 0.5f, 0.0f),        // Position
                Vector3(0.0f, 0.0f, 0.0f),         // Normal
                Vector2(0, 1),                     // Texture coordinates
                RgbaColor(0.0f, 0.0f, 0.0f, 1.0f)  // Color
            } 
        };

        const IndexBuffer indexBuffer = { 0, 1, 2, 2, 3, 0 };
        const VertexBuffer vertexBuffer =
        {
            quadMeshVerts,
            {{
                // Pos
                Vector3(),
                // Vert Color
                RgbaColor(),
                // Normal
                Vector3(),
                // Tex Coord
                Vector2(),

            }}
        };

        return { vertexBuffer, indexBuffer };
    }
}