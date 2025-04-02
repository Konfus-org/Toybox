#include "Tbx/Core/PCH.h"
#include "Tbx/Core/Rendering/Mesh.h"

namespace Tbx
{
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
}