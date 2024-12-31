#include "OpenGLBuffers.h"


namespace OpenGLRendering
{
    /////////////////////////////////////////////////////////////////////////////
    /// Vertex Buffer ///////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////

    OpenGLVertexBuffer::OpenGLVertexBuffer()
    {
        _count = 0;
        glCreateBuffers(1, &_rendererId);
    }

    OpenGLVertexBuffer::~OpenGLVertexBuffer()
    {
        glDeleteBuffers(1, &_rendererId);
    }

    void OpenGLVertexBuffer::SetData(const Toybox::VertexBuffer& vertices)
    {
        const auto& verticesVec = vertices.GetVertices();
        const auto& sizeofVertices = sizeof(verticesVec);
        _count = (Toybox::uint32)verticesVec.size();
        glBufferData(GL_ARRAY_BUFFER, sizeofVertices, verticesVec.data(), GL_STATIC_DRAW);
    }

    void OpenGLVertexBuffer::Bind() const
    {
        glBindBuffer(GL_ARRAY_BUFFER, _rendererId);
    }

    void OpenGLVertexBuffer::Unbind() const
    {
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void OpenGLVertexBuffer::AddAttribute(Toybox::uint index, Toybox::uint size, Toybox::uint type, Toybox::uint stride, bool normalized) const
    {
        glEnableVertexAttribArray(index);
        glVertexAttribPointer(index, size, type, normalized, stride, nullptr);
    }

    /////////////////////////////////////////////////////////////////////////////
    /// Index Buffer ////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////

    OpenGLIndexBuffer::OpenGLIndexBuffer()
    {
        _count = 0;
        glCreateBuffers(1, &_rendererId);
    }

    void OpenGLIndexBuffer::SetData(const Toybox::IndexBuffer& indices)
    {
        const auto& indicesVec = indices.GetIndices();
        const auto& sizeofIndices = sizeof(indicesVec);
        _count = (Toybox::uint32)indicesVec.size();
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeofIndices, indicesVec.data(), GL_STATIC_DRAW);
    }

    OpenGLIndexBuffer::~OpenGLIndexBuffer()
    {
        glDeleteBuffers(1, &_rendererId);
    }

    void OpenGLIndexBuffer::Bind() const
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _rendererId);
    }

    void OpenGLIndexBuffer::Unbind() const
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
}
