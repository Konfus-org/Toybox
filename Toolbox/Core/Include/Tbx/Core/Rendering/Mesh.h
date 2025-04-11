#pragma once
#include "Tbx/Core/Rendering/Buffers.h"
#include "Tbx/Core/Rendering/Vertex.h"
#include "Tbx/Core/Rendering/Material.h"
#include "Tbx/Core/Rendering/Color.h"
#include "Tbx/Core/Math/Int.h"

namespace Tbx
{
    struct Mesh
    {
    public:
        EXPORT Mesh() = default;
        EXPORT Mesh(const std::initializer_list<Vertex>& vertices, const std::initializer_list<uint32>& indices);
        EXPORT Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32>& indices);
        EXPORT Mesh(const VertexBuffer& vertices, const IndexBuffer& indices)
            : _vertexBuffer(vertices), _indexBuffer(indices) {}

        EXPORT const VertexBuffer& GetVertexBuffer() const { return _vertexBuffer; }
        EXPORT const IndexBuffer& GetIndexBuffer() const { return _indexBuffer; }

        EXPORT void SetVertexBuffer(const VertexBuffer& vertices) { _vertexBuffer = vertices; }
        EXPORT void SetIndexBuffer(const IndexBuffer& indices) { _indexBuffer = indices; }

        EXPORT static Mesh MakeTriangle();
        EXPORT static Mesh MakeQuad();

    private:
        VertexBuffer VertexVectorToBuffer(const std::vector<Vertex>& vertices) const;

        VertexBuffer _vertexBuffer;  
        IndexBuffer _indexBuffer;
    };

    struct MeshRenderData
    {
        EXPORT MeshRenderData(const Mesh& mesh, const Material& material) 
            : _mesh(mesh), _material(material) {}
        EXPORT ~MeshRenderData() = default;

        EXPORT const Mesh& GetMesh() const { return _mesh; }
        EXPORT const Material& GetMaterial() const { return _material; }

    private:
        Mesh _mesh;
        Material _material;
    };
}

