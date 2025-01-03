#include "tbxpch.h"
#include "Mesh.h"

namespace Toybox
{
    Mesh Mesh::MakeTriangle(const Color& color)
    {
        const auto& meshVerts = 
        {
            Toybox::Vertex(Vector3(-0.5f, -0.5f, 0.0f), color),
            Toybox::Vertex(Vector3(0.5f, -0.5f, 0.0f), color),
            Toybox::Vertex(Vector3(0.0f, 0.5f, 0.0f), color)
        };
        const std::vector<uint32>& meshIndices = { 0, 1, 2 };
        const auto& mesh = Mesh(meshVerts, meshIndices);
        return mesh;
    }
}