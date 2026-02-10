#include "opengl_buffers.h"
#include "tbx/debugging/macros.h"
#include <glad/glad.h>
#include <utility>
#include <variant>

namespace tbx::plugins
{
    OpenGlFrameBuffer::~OpenGlFrameBuffer() noexcept
    {
        if (_depth_stencil_renderbuffer_id != 0)
        {
            glDeleteRenderbuffers(1, &_depth_stencil_renderbuffer_id);
            _depth_stencil_renderbuffer_id = 0;
        }

        if (_color_texture_id != 0)
        {
            glDeleteTextures(1, &_color_texture_id);
            _color_texture_id = 0;
        }

        if (_framebuffer_id != 0)
        {
            glDeleteFramebuffers(1, &_framebuffer_id);
            _framebuffer_id = 0;
        }
    }

    OpenGlFrameBuffer::OpenGlFrameBuffer(OpenGlFrameBuffer&& other) noexcept
        : _framebuffer_id(std::exchange(other._framebuffer_id, 0))
        , _color_texture_id(std::exchange(other._color_texture_id, 0))
        , _depth_stencil_renderbuffer_id(std::exchange(other._depth_stencil_renderbuffer_id, 0))
        , _size(std::exchange(other._size, {}))
    {
    }

    void OpenGlFrameBuffer::bind() const
    {
        glBindFramebuffer(GL_FRAMEBUFFER, _framebuffer_id);
    }

    void OpenGlFrameBuffer::unbind() const
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    Size OpenGlFrameBuffer::get_size() const
    {
        return _size;
    }

    uint32 OpenGlFrameBuffer::get_framebuffer_id() const
    {
        return _framebuffer_id;
    }

    uint32 OpenGlFrameBuffer::get_color_texture_id() const
    {
        return _color_texture_id;
    }

    void OpenGlFrameBuffer::set_size(const Size& size)
    {
        if (_framebuffer_id == 0)
        {
            glCreateFramebuffers(1, &_framebuffer_id);
        }

        if (_color_texture_id != 0)
        {
            glDeleteTextures(1, &_color_texture_id);
        }
        if (_depth_stencil_renderbuffer_id != 0)
        {
            glDeleteRenderbuffers(1, &_depth_stencil_renderbuffer_id);
        }

        glCreateTextures(GL_TEXTURE_2D, 1, &_color_texture_id);
        glTextureParameteri(_color_texture_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(_color_texture_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(_color_texture_id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(_color_texture_id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureStorage2D(
            _color_texture_id,
            1,
            GL_RGBA8,
            static_cast<GLsizei>(size.width),
            static_cast<GLsizei>(size.height));
        glNamedFramebufferTexture(_framebuffer_id, GL_COLOR_ATTACHMENT0, _color_texture_id, 0);

        glCreateRenderbuffers(1, &_depth_stencil_renderbuffer_id);
        glNamedRenderbufferStorage(
            _depth_stencil_renderbuffer_id,
            GL_DEPTH24_STENCIL8,
            static_cast<GLsizei>(size.width),
            static_cast<GLsizei>(size.height));
        glNamedFramebufferRenderbuffer(
            _framebuffer_id,
            GL_DEPTH_STENCIL_ATTACHMENT,
            GL_RENDERBUFFER,
            _depth_stencil_renderbuffer_id);

        const GLenum draw_buffers[] = {GL_COLOR_ATTACHMENT0};
        glNamedFramebufferDrawBuffers(_framebuffer_id, 1, draw_buffers);

        GLenum status = glCheckNamedFramebufferStatus(_framebuffer_id, GL_FRAMEBUFFER);
        TBX_ASSERT(
            status == GL_FRAMEBUFFER_COMPLETE,
            "OpenGL rendering: framebuffer is incomplete (status={}).",
            static_cast<uint32>(status));

        _size = size;
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
        if (std::holds_alternative<RgbaColor>(type))
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
            add_attribute(
                index,
                element.count,
                static_cast<uint32>(type),
                stride,
                element.offset,
                element.normalized);
            index += 1;
        }
    }

    void OpenGlVertexBuffer::add_attribute(
        uint32 index,
        uint32 size,
        uint32 type,
        uint32 stride,
        uint32 offset,
        bool normalized) const
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

    void OpenGlVertexBuffer::bind() const
    {
        glBindBuffer(GL_ARRAY_BUFFER, _buffer_id);
    }

    void OpenGlVertexBuffer::unbind() const
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

    void OpenGlIndexBuffer::bind() const
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _buffer_id);
    }

    void OpenGlIndexBuffer::unbind() const
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    uint32 OpenGlIndexBuffer::get_count() const
    {
        return _count;
    }
}
