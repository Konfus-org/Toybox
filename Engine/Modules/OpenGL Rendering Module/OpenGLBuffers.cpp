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

    void OpenGLVertexBuffer::SetLayout(const Toybox::BufferLayout& layout) const
    {
        Toybox::uint32 index = 0;
        for (const auto& element : layout)
        {
            AddAttribute(
                index, 
                element.GetCount(), 
                ShaderDataTypeToOpenGLType(element.GetType()),
                layout.GetStride(),
                element.GetOffset(),
                element.IsNormalized());
            index++;
        }
    }

    void OpenGLVertexBuffer::AddAttribute(const Toybox::uint& index, const Toybox::uint& size, const Toybox::uint& type, const Toybox::uint& stride, const Toybox::uint& offset, const bool& normalized) const
    {
        glEnableVertexAttribArray(index);
#pragma warning( push )
#pragma warning( disable : 4312 )
        glVertexAttribPointer(index, size, type, normalized, stride, (const void*)offset);
#pragma warning( pop )
    }

    void OpenGLVertexBuffer::Bind() const
    {
        glBindBuffer(GL_ARRAY_BUFFER, _rendererId);
    }

    void OpenGLVertexBuffer::Unbind() const
    {
        glBindBuffer(GL_ARRAY_BUFFER, 0);
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
