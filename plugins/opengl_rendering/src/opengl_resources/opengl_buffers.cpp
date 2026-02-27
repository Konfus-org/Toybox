#include "opengl_buffers.h"
#include "tbx/debugging/macros.h"
#include <glad/glad.h>
#include <variant>

namespace opengl_rendering
{
    using namespace tbx;
    static void add_attribute(
        const uint32 index,
        const uint32 size,
        const uint32 type,
        const uint32 stride,
        const uint32 offset,
        const bool normalized)
    {
        glEnableVertexAttribArray(index);
        const auto* attribute_offset =
            reinterpret_cast<const void*>(static_cast<std::uintptr_t>(offset));
        if (type == GL_INT && !normalized)
        {
            glVertexAttribIPointer(index, size, type, stride, attribute_offset);
        }
        else
        {
            glVertexAttribPointer(
                index,
                size,
                type,
                normalized ? GL_TRUE : GL_FALSE,
                stride,
                attribute_offset);
        }
    }

    static GLenum vertex_type_to_gl_type(const VertexData& type)
    {
        if (std::holds_alternative<Vec2>(type))
        {
            return GL_FLOAT;
        }
        if (std::holds_alternative<Vec3>(type))
        {
            return GL_FLOAT;
        }
        if (std::holds_alternative<Color>(type))
        {
            return GL_FLOAT;
        }
        if (std::holds_alternative<float>(type))
        {
            return GL_FLOAT;
        }
        if (std::holds_alternative<int>(type))
        {
            return GL_INT;
        }

        TBX_ASSERT(false, "OpenGL rendering: could not convert vertex data to OpenGL type.");
        return GL_NONE;
    }

    OpenGlVertexBuffer::OpenGlVertexBuffer()
    {
        glCreateBuffers(1, &_buffer_id);
    }

    OpenGlVertexBuffer::~OpenGlVertexBuffer() noexcept
    {
        if (_buffer_id != 0)
        {
            glDeleteBuffers(1, &_buffer_id);
        }
    }

    void OpenGlVertexBuffer::upload(const VertexBuffer& buffer)
    {
        _count = static_cast<uint32>(buffer.vertices.size());
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(_count * sizeof(float)),
            buffer.vertices.data(),
            GL_STATIC_DRAW);

        uint32 index = 0;
        const auto& layout = buffer.layout;
        const auto& stride = layout.stride;
        for (const auto& element : layout.elements)
        {
            const auto type = vertex_type_to_gl_type(element.type);
            add_attribute(index, element.count, type, stride, element.offset, element.normalized);
            index += 1;
        }
    }

    void OpenGlVertexBuffer::bind()
    {
        glBindBuffer(GL_ARRAY_BUFFER, _buffer_id);
    }

    void OpenGlVertexBuffer::unbind()
    {
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    uint32 OpenGlVertexBuffer::get_count() const
    {
        return _count;
    }

    OpenGlIndexBuffer::OpenGlIndexBuffer()
    {
        glCreateBuffers(1, &_buffer_id);
    }

    OpenGlIndexBuffer::~OpenGlIndexBuffer() noexcept
    {
        if (_buffer_id != 0)
        {
            glDeleteBuffers(1, &_buffer_id);
        }
    }

    void OpenGlIndexBuffer::upload(const IndexBuffer& buffer)
    {
        _count = static_cast<uint32>(buffer.size());
        glBufferData(
            GL_ELEMENT_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(_count * sizeof(uint32)),
            buffer.data(),
            GL_STATIC_DRAW);
    }

    void OpenGlIndexBuffer::bind()
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _buffer_id);
    }

    void OpenGlIndexBuffer::unbind()
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    uint32 OpenGlIndexBuffer::get_count() const
    {
        return _count;
    }
}
