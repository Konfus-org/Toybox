#pragma once
#include "Tbx/Core/Rendering/Buffers.h"
#include "Tbx/Core/Rendering/Vertex.h"
#include "Tbx/Core/Rendering/Material.h"
#include "Tbx/Core/Rendering/Color.h"
#include "Tbx/Core/Math/Int.h"

namespace Tbx
{
    // TODO: Mesh should be a "Block" or component
    struct Mesh
    {
    public:
        EXPORT static Mesh MakeTriangle(const Color& color = Color(1.0f, 1.0f, 1.0f, 1.0f));

        EXPORT Mesh() = default;
        EXPORT Mesh(const std::initializer_list<Vertex>& vertices, const std::initializer_list<uint32>& indices)
            : _vertexBuffer(VertexVectorToBuffer(vertices)), _indexBuffer(indices) {}
        EXPORT Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32>& indices)
            : _vertexBuffer(VertexVectorToBuffer(vertices)), _indexBuffer(indices) {}
        EXPORT Mesh(const VertexBuffer& vertices, const IndexBuffer& indices)
            : _vertexBuffer(vertices), _indexBuffer(indices) {}

        EXPORT VertexBuffer GetVertexBuffer() const { return _vertexBuffer; }
        EXPORT IndexBuffer GetIndexBuffer() const { return _indexBuffer; }

    private:
        VertexBuffer _vertexBuffer;  
        IndexBuffer _indexBuffer;
    };

    struct MeshRenderData
    {
        EXPORT MeshRenderData(const Mesh& mesh, const Material& material) : _mesh(mesh), _material(material) {}
        EXPORT ~MeshRenderData() = default;

        EXPORT const Mesh& GetMesh() const { return _mesh; }
        EXPORT const Material& GetMaterial() const { return _material; }

    private:
        Mesh _mesh;
        Material _material;
    };
}

