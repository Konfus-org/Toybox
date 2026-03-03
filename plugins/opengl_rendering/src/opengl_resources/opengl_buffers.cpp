#include "opengl_buffers.h"
#include "tbx/debugging/macros.h"
#include <glad/glad.h>
#include <utility>
#include <variant>

namespace opengl_rendering
{
    static tbx::uint32 take_gl_handle(tbx::uint32& id) noexcept
    {
        return std::exchange(id, 0);
    }

    static void add_attribute(
        const tbx::uint32 vertex_array_id,
        const tbx::uint32 index,
        const tbx::uint32 size,
        const tbx::uint32 type,
        const tbx::uint32 offset,
        const bool normalized)
    {
        glEnableVertexArrayAttrib(vertex_array_id, index);
        if (type == GL_INT && !normalized)
            glVertexArrayAttribIFormat(vertex_array_id, index, size, type, offset);
        else
            glVertexArrayAttribFormat(
                vertex_array_id,
                index,
                size,
                type,
                normalized ? GL_TRUE : GL_FALSE,
                offset);

        glVertexArrayAttribBinding(vertex_array_id, index, 0);
    }

    static GLenum vertex_type_to_gl_type(const tbx::VertexData& type)
    {
        if (std::holds_alternative<tbx::Vec2>(type))
        {
            return GL_FLOAT;
        }
        if (std::holds_alternative<tbx::Vec3>(type))
        {
            return GL_FLOAT;
        }
        if (std::holds_alternative<tbx::Color>(type))
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

    OpenGlVertexBuffer::OpenGlVertexBuffer(OpenGlVertexBuffer&& other) noexcept
        : _buffer_id(take_gl_handle(other._buffer_id))
        , _count(other._count)
    {
        other._count = 0;
    }

    OpenGlVertexBuffer& OpenGlVertexBuffer::operator=(OpenGlVertexBuffer&& other) noexcept
    {
        if (this == &other)
            return *this;

        if (_buffer_id != 0)
            glDeleteBuffers(1, &_buffer_id);

        _buffer_id = take_gl_handle(other._buffer_id);
        _count = other._count;
        other._count = 0;
        return *this;
    }

    OpenGlVertexBuffer::~OpenGlVertexBuffer() noexcept
    {
        if (_buffer_id != 0)
        {
            glDeleteBuffers(1, &_buffer_id);
        }
    }

    void OpenGlVertexBuffer::upload(
        const tbx::uint32 vertex_array_id,
        const tbx::VertexBuffer& buffer)
    {
        _count = static_cast<tbx::uint32>(buffer.vertices.size());
        glNamedBufferData(
            _buffer_id,
            static_cast<GLsizeiptr>(_count * sizeof(float)),
            buffer.vertices.data(),
            GL_STATIC_DRAW);

        const auto& layout = buffer.layout;
        TBX_ASSERT(
            layout.stride > 0,
            "OpenGL rendering: vertex buffer layout stride must be greater than zero.");
        glVertexArrayVertexBuffer(vertex_array_id, 0, _buffer_id, 0, layout.stride);

        tbx::uint32 index = 0;
        for (const auto& element : layout.elements)
        {
            const auto type = vertex_type_to_gl_type(element.type);
            add_attribute(
                vertex_array_id,
                index,
                element.count,
                type,
                element.offset,
                element.normalized);
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

    tbx::uint32 OpenGlVertexBuffer::get_count() const
    {
        return _count;
    }

    OpenGlIndexBuffer::OpenGlIndexBuffer()
    {
        glCreateBuffers(1, &_buffer_id);
    }

    OpenGlIndexBuffer::OpenGlIndexBuffer(OpenGlIndexBuffer&& other) noexcept
        : _buffer_id(take_gl_handle(other._buffer_id))
        , _count(other._count)
    {
        other._count = 0;
    }

    OpenGlIndexBuffer& OpenGlIndexBuffer::operator=(OpenGlIndexBuffer&& other) noexcept
    {
        if (this == &other)
            return *this;

        if (_buffer_id != 0)
            glDeleteBuffers(1, &_buffer_id);

        _buffer_id = take_gl_handle(other._buffer_id);
        _count = other._count;
        other._count = 0;
        return *this;
    }

    OpenGlIndexBuffer::~OpenGlIndexBuffer() noexcept
    {
        if (_buffer_id != 0)
        {
            glDeleteBuffers(1, &_buffer_id);
        }
    }

    void OpenGlIndexBuffer::upload(
        const tbx::uint32 vertex_array_id,
        const tbx::IndexBuffer& buffer)
    {
        _count = static_cast<tbx::uint32>(buffer.size());
        glNamedBufferData(
            _buffer_id,
            static_cast<GLsizeiptr>(_count * sizeof(tbx::uint32)),
            buffer.data(),
            GL_STATIC_DRAW);
        glVertexArrayElementBuffer(vertex_array_id, _buffer_id);
    }

    void OpenGlIndexBuffer::bind()
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _buffer_id);
    }

    void OpenGlIndexBuffer::unbind()
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    tbx::uint32 OpenGlIndexBuffer::get_count() const
    {
        return _count;
    }
}
