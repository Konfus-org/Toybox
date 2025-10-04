#include "OpenGLBuffers.h"
#include <Tbx/Debug/Tracers.h>
#include <glad/glad.h>

namespace OpenGLRendering
{
    /// Helpers ///////////////////////////////////////////////////////////

    static GLenum VertexTypeToGlType(const Tbx::VertexData& type)
    {
        if (std::holds_alternative<Tbx::Vector2>(type))
        {
            return GL_FLOAT;
        }
        else if (std::holds_alternative<Tbx::Vector3>(type))
        {
            return GL_FLOAT;
        }
        else if (std::holds_alternative<Tbx::RgbaColor>(type))
        {
            return GL_FLOAT;
        }
        else if (std::holds_alternative<float>(type))
        {
            return GL_FLOAT;
        }
        else if (std::holds_alternative<int>(type))
        {
            return GL_INT;
        }
        else
        {
            TBX_ASSERT(false, "GL Rendering: Could not convert to OpenGL type from vertex data, given unknown data type!");
            return GL_NONE;
        }

    }

    /// Vertex Buffer ///////////////////////////////////////////////////////////

    OpenGLVertexBuffer::OpenGLVertexBuffer()
    {
        glCreateBuffers(1, &_vertBufferGLId);
    }

    OpenGLVertexBuffer::~OpenGLVertexBuffer()
    {
        Unbind();
        glDeleteBuffers(1, &_vertBufferGLId);
    }

    void OpenGLVertexBuffer::Upload(const Tbx::VertexBuffer& buffer)
    {
        const auto& verticesVec = buffer.Vertices;
        _count = (Tbx::uint32)verticesVec.size();
        glBufferData(GL_ARRAY_BUFFER, _count * sizeof(float), verticesVec.data(), GL_STATIC_DRAW);

        Tbx::uint32 index = 0;
        const auto& layout = buffer.Layout;
        const auto& stride = layout.Stride;
        for (const auto& element : layout.Elements)
        {
            const auto& type = VertexTypeToGlType(element.Type);
            const auto& size = element.Count;
            const auto& offset = element.Offset;
            const auto& normalized = element.Normalized ? GL_TRUE : GL_FALSE;
            AddAttribute(index, size, type, stride, offset, normalized);
            index++;
        }
    }

    void OpenGLVertexBuffer::AddAttribute(Tbx::uint index, Tbx::uint32 size, Tbx::uint32 type, Tbx::uint32 stride, Tbx::uint32 offset, bool normalized) const
    {
        // Need to do this casting nonsense to get rid of a warning...
        const auto* offsetPtr = reinterpret_cast<const void*>(static_cast<std::uintptr_t>(offset));
        glEnableVertexAttribArray(index);
        glVertexAttribPointer(index, size, type, normalized, stride, offsetPtr);
    }

    void OpenGLVertexBuffer::Bind() const
    {
        glBindBuffer(GL_ARRAY_BUFFER, _vertBufferGLId);
    }

    void OpenGLVertexBuffer::Unbind() const
    {
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    /// Index Buffer ////////////////////////////////////////////////////////////

    OpenGLIndexBuffer::OpenGLIndexBuffer()
    {
        glCreateBuffers(1, &_indexBuffGLId);
    }

    void OpenGLIndexBuffer::Upload(const Tbx::IndexBuffer& buffer)
    {
        _count = (Tbx::uint32)buffer.size();
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, _count * sizeof(Tbx::uint32), buffer.data(), GL_STATIC_DRAW);
    }

    OpenGLIndexBuffer::~OpenGLIndexBuffer()
    {
        Unbind();
        glDeleteBuffers(1, &_indexBuffGLId);
    }

    void OpenGLIndexBuffer::Bind() const
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _indexBuffGLId);
    }

    void OpenGLIndexBuffer::Unbind() const
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
}
