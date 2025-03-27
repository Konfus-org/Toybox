#pragma once
#include "Buffers.h"
#include "Vertex.h"
#include "Material.h"
#include "Color.h"
#include "Math/Int.h"

namespace Tbx
{
    // TODO: Mesh should be a "Block" or component
    struct Mesh
    {
    public:
        TBX_API static Mesh MakeTriangle(const Color& color = Color(1.0f, 1.0f, 1.0f, 1.0f));

        TBX_API Mesh() = default;
        TBX_API Mesh(const std::initializer_list<Vertex>& vertices, const std::initializer_list<uint32>& indices)
            : _vertexBuffer(VertexVectorToBuffer(vertices)), _indexBuffer(indices) {}
        TBX_API Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32>& indices)
            : _vertexBuffer(VertexVectorToBuffer(vertices)), _indexBuffer(indices) {}
        TBX_API Mesh(const VertexBuffer& vertices, const IndexBuffer& indices)
            : _vertexBuffer(vertices), _indexBuffer(indices) {}

        TBX_API VertexBuffer GetVertexBuffer() const { return _vertexBuffer; }
        TBX_API IndexBuffer GetIndexBuffer() const { return _indexBuffer; }

    private:
        VertexBuffer _vertexBuffer;  
        IndexBuffer _indexBuffer;
    };

    struct MeshRenderData
    {
        TBX_API MeshRenderData(const Mesh& mesh, const Material& material) : _mesh(mesh), _material(material) {}
        TBX_API ~MeshRenderData() = default;

        TBX_API const Mesh& GetMesh() const { return _mesh; }
        TBX_API const Material& GetMaterial() const { return _material; }

    private:
        Mesh _mesh;
        Material _material;
    };
}

