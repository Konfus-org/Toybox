#include "OpenGLBuffers.h"
#include "OpenGLShader.h"


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
        _count = (Toybox::uint32)verticesVec.size();
        glBindBuffer(GL_ARRAY_BUFFER, _rendererId);
        glBufferData(GL_ARRAY_BUFFER, _count * sizeof(float), vertices.GetVertices().data(), GL_STATIC_DRAW);

        Toybox::uint32 index = 0;
        const auto& layout = vertices.GetLayout();
        for (const auto& element : layout)
        {
            const auto& count = element.GetCount();
            const auto& type = ShaderDataTypeToOpenGLType(element.GetType());
            const auto& stride = layout.GetStride();
            const auto& offset = element.GetOffset();
            const auto& normalized = element.IsNormalized() ? GL_TRUE : GL_FALSE;

            AddAttribute(index, count, type, stride, offset, normalized);

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
        _count = (Toybox::uint32)indicesVec.size();
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, _count * sizeof(Toybox::uint32), indicesVec.data(), GL_STATIC_DRAW);
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

    /////////////////////////////////////////////////////////////////////////////
    /// Vertex Array ////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////

    OpenGLVertexArray::OpenGLVertexArray()
    {
        glGenVertexArrays(1, &_rendererId);
    }

    OpenGLVertexArray::~OpenGLVertexArray()
    {
        glDeleteBuffers(1, &_rendererId);
    }

    void OpenGLVertexArray::AddVertexBuffer(const Toybox::VertexBuffer& buffer)
    {
        TBX_ASSERT(buffer.GetLayout().GetElements().size(), "Vertex buffer has no layout... a layout MUST be provided!");

        auto& glBuffer = _vertexBuffers.emplace_back();
        glBuffer.Bind();
        glBuffer.SetData(buffer);
    }

    void OpenGLVertexArray::SetIndexBuffer(const Toybox::IndexBuffer& buffer)
    {
        _indexBuffer.Bind();
        _indexBuffer.SetData(buffer);
    }

    void OpenGLVertexArray::Bind() const
    {
        glBindVertexArray(_rendererId);
    }

    void OpenGLVertexArray::Unbind() const
    {
        glBindVertexArray(0);
    }
}
