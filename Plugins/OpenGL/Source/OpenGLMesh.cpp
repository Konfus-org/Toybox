#include "OpenGLMesh.h"
#include "OpenGLBuffers.h"
#include <glad/glad.h>

namespace Tbx::Plugins::OpenGLRendering
{
    OpenGLRendering::OpenGLMesh::OpenGLMesh(const Mesh& mesh)
    {
        auto id = static_cast<uint32>(RenderId);
        glGenVertexArrays(1, &id);
        RenderId = id;

        SetVertexBuffer(mesh.Vertices);
        SetIndexBuffer(mesh.Indices);
    }

    OpenGLMesh::~OpenGLMesh()
    {
        auto id = static_cast<uint32>(RenderId);
        glDeleteVertexArrays(1, &id);
    }

    void OpenGLMesh::Draw()
    {
        glDrawElements(GL_TRIANGLES, _indexBuffer.GetCount(), GL_UNSIGNED_INT, 0);
    }

    void OpenGLMesh::SetVertexBuffer(const VertexBuffer& buffer)
    {
        glBindVertexArray(RenderId);
        TBX_ASSERT(buffer.Vertices.size(), "GL Rendering: Vertex buffer must not be empty!");
        TBX_ASSERT(buffer.Layout.Elements.size(), "GL Rendering: Vertex buffer must provide a layout!");
        _vertexBuffer.Bind();
        _vertexBuffer.Upload(buffer);
    }

    void OpenGLMesh::SetIndexBuffer(const IndexBuffer& buffer)
    {
        glBindVertexArray(RenderId);
        TBX_ASSERT(buffer.size(), "GL Rendering: Index buffer must not be empty!");
        _indexBuffer.Bind();
        _indexBuffer.Upload(buffer);
    }

    void OpenGLMesh::Activate()
    {
        glBindVertexArray(RenderId);
        _vertexBuffer.Bind();
        _indexBuffer.Bind();
    }

    void OpenGLMesh::Release()
    {
        glBindVertexArray(0);
        _vertexBuffer.Unbind();
        _indexBuffer.Unbind();
    }
}