#pragma once
#include "tbxpch.h"
#include "Buffers.h"
#include "Vertex.h"
#include "Color.h"
#include "Math/Int.h"

namespace Toybox
{
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
}

