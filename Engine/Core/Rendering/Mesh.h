#pragma once
#include "tbxpch.h"
#include "Buffers.h"
#include "Vertex.h"
#include "Math/Int.h"

namespace Toybox
{
    struct Mesh
    {
    public:
        TBX_API static Mesh Triangle()
        { 
            return Mesh(
                { 
                    Toybox::Vertex({ -0.5f, -0.5f, 0.0f }), 
                    Toybox::Vertex({ 0.5f, -0.5f, 0.0f }), 
                    Toybox::Vertex({ 0.0f, 0.5f, 0.0f }) 
                }, 
                { 0, 1, 2 }
            );
        }

        TBX_API static Mesh Quad()
        {
            return Mesh(
                {
                    Toybox::Vertex({ -0.5f, -0.5f, 0.0f }),
                    Toybox::Vertex({ 0.5f, -0.5f, 0.0f }),
                    Toybox::Vertex({ 0.5f, 0.5f, 0.0f }),
                    Toybox::Vertex({ -0.5f, 0.5f, 0.0f }),
                },
                { 0, 1, 2, 2, 3, 0 }
            );
        }

        TBX_API static Mesh Cube()
        {
            return Mesh(
                {
                    Toybox::Vertex({ -1.0f, -1.0f, -1.0f }),
                    Toybox::Vertex({ 1.0f, -1.0f, -1.0f }),
                    Toybox::Vertex({ 1.0f, 1.0f, -1.0f }),
                    Toybox::Vertex({ -1.0f, 1.0f, -1.0f }),
                    Toybox::Vertex({ -1.0f, -1.0f, 1.0f }),
                    Toybox::Vertex({ 1.0f, -1.0f, 1.0f }),
                    Toybox::Vertex({ 1.0f, 1.0f, 1.0f }),
                    Toybox::Vertex({ -1.0f, 1.0f, 1.0f }),
                },
                { 0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4 }
            );
        }

        TBX_API Mesh() = default;
        TBX_API Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32>& indices)
            : _vertexBuffer(vertices), _indexBuffer(indices) {}
        TBX_API Mesh(const VertexBuffer& vertices, const IndexBuffer& indices)
            : _vertexBuffer(vertices), _indexBuffer(indices) {}

        TBX_API inline VertexBuffer GetVertexBuffer() const { return _vertexBuffer; }
        TBX_API inline IndexBuffer GetIndexBuffer() const { return _indexBuffer; }

    private:
        VertexBuffer _vertexBuffer;  
        IndexBuffer _indexBuffer;
    };
}

