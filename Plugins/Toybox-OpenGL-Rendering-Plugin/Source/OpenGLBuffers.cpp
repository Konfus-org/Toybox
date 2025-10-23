#include "OpenGLBuffers.h"
#include <Tbx/Debug/Tracers.h>
#include <glad/glad.h>
#include <cstdint>

namespace Tbx::Plugins::OpenGLRendering
{
    /// Helpers ///////////////////////////////////////////////////////////

    static GLenum VertexTypeToGlType(const VertexData& type)
    {
        if (std::holds_alternative<Vector2>(type))
        {
            return GL_FLOAT;
        }
        else if (std::holds_alternative<Vector3>(type))
        {
            return GL_FLOAT;
        }
        else if (std::holds_alternative<RgbaColor>(type))
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

    void OpenGLVertexBuffer::Upload(const VertexBuffer& buffer)
    {
        const auto& verticesVec = buffer.Vertices;
        _count = (uint32)verticesVec.size();
        glBufferData(GL_ARRAY_BUFFER, _count * sizeof(float), verticesVec.data(), GL_STATIC_DRAW);

        uint32 index = 0;
        const auto& layout = buffer.Layout;
        const auto& stride = layout.Stride;
        for (const auto& element : layout.Elements)
        {
            const auto& type = VertexTypeToGlType(element.Type);
            const auto& size = element.Count;
            const auto& offset = element.Offset;
            const bool normalized = element.Normalized;
            AddAttribute(index, size, type, stride, offset, normalized);
            index++;
        }
    }

    void OpenGLVertexBuffer::AddAttribute(uint index, uint32 size, uint32 type, uint32 stride, uint32 offset, bool normalized) const
    {
        // Need to do this casting nonsense to get rid of a warning...
        glEnableVertexAttribArray(index);
        const void* attributeOffset = reinterpret_cast<const void*>(static_cast<uintptr_t>(offset));
        if (type == GL_INT && !normalized)
        {
            glVertexAttribIPointer(index, size, type, stride, attributeOffset);
        }
        else
        {
            glVertexAttribPointer(index, size, type, normalized ? GL_TRUE : GL_FALSE, stride, attributeOffset);
        }
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

    OpenGLIndexBuffer::~OpenGLIndexBuffer()
    {
        Unbind();
        glDeleteBuffers(1, &_indexBuffGLId);
    }

    void OpenGLIndexBuffer::Upload(const IndexBuffer& buffer)
    {
        _count = (uint32)buffer.size();
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, _count * sizeof(uint32), buffer.data(), GL_STATIC_DRAW);
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
