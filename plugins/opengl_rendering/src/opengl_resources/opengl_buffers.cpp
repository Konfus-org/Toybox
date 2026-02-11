#include "opengl_buffers.h"
#include "tbx/debugging/macros.h"
#include <glad/glad.h>
#include <variant>

namespace tbx::plugins
{
    static GLenum to_gl_texture_filter(const TextureFilter filtering)
    {
        switch (filtering)
        {
            case TextureFilter::NEAREST:
                return GL_NEAREST;
            case TextureFilter::LINEAR:
                return GL_LINEAR;
            default:
                TBX_ASSERT(false, "OpenGL rendering: unsupported framebuffer texture filter.");
                return GL_LINEAR;
        }
    }

    static void get_preset_destination_bounds(
        const Size& source_size,
        const Size& destination_size,
        const OpenGlFrameBufferPresentMode mode,
        GLint& out_x,
        GLint& out_y,
        GLint& out_width,
        GLint& out_height)
    {
        out_x = 0;
        out_y = 0;
        out_width = static_cast<GLint>(destination_size.width);
        out_height = static_cast<GLint>(destination_size.height);
        if (mode == OpenGlFrameBufferPresentMode::STRETCH)
            return;

        const auto source_width = static_cast<float>(source_size.width);
        const auto source_height = static_cast<float>(source_size.height);
        const auto destination_width = static_cast<float>(destination_size.width);
        const auto destination_height = static_cast<float>(destination_size.height);
        const auto source_aspect = source_width / source_height;
        const auto destination_aspect = destination_width / destination_height;

        if (source_aspect > destination_aspect)
        {
            out_height = static_cast<GLint>(destination_width / source_aspect);
            out_y = (static_cast<GLint>(destination_size.height) - out_height) / 2;
            return;
        }

        if (source_aspect < destination_aspect)
        {
            out_width = static_cast<GLint>(destination_height * source_aspect);
            out_x = (static_cast<GLint>(destination_size.width) - out_width) / 2;
        }
    }

    OpenGlFrameBuffer::OpenGlFrameBuffer(const Size& resolution)
    {
        set_resolution(resolution);
    }

    OpenGlFrameBuffer::OpenGlFrameBuffer(const Size& resolution, const TextureFilter filtering)
        : _filtering(filtering)
    {
        set_resolution(resolution);
    }

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

    void OpenGlFrameBuffer::bind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, _framebuffer_id);
    }

    void OpenGlFrameBuffer::unbind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    Size OpenGlFrameBuffer::get_resolution() const
    {
        return _resolution;
    }

    uint32 OpenGlFrameBuffer::get_framebuffer_id() const
    {
        return _framebuffer_id;
    }

    uint32 OpenGlFrameBuffer::get_color_texture_id() const
    {
        return _color_texture_id;
    }

    void OpenGlFrameBuffer::set_filtering(const TextureFilter filtering)
    {
        _filtering = filtering;
        if (_color_texture_id == 0)
            return;

        const auto gl_filter = to_gl_texture_filter(_filtering);
        glTextureParameteri(_color_texture_id, GL_TEXTURE_MIN_FILTER, gl_filter);
        glTextureParameteri(_color_texture_id, GL_TEXTURE_MAG_FILTER, gl_filter);
    }

    TextureFilter OpenGlFrameBuffer::get_filtering() const
    {
        return _filtering;
    }

    void OpenGlFrameBuffer::set_resolution(const Size& resolution)
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

        const auto gl_filter = to_gl_texture_filter(_filtering);
        glCreateTextures(GL_TEXTURE_2D, 1, &_color_texture_id);
        glTextureParameteri(_color_texture_id, GL_TEXTURE_MIN_FILTER, gl_filter);
        glTextureParameteri(_color_texture_id, GL_TEXTURE_MAG_FILTER, gl_filter);
        glTextureParameteri(_color_texture_id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(_color_texture_id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureStorage2D(
            _color_texture_id,
            1,
            GL_RGBA8,
            static_cast<GLsizei>(resolution.width),
            static_cast<GLsizei>(resolution.height));
        glNamedFramebufferTexture(_framebuffer_id, GL_COLOR_ATTACHMENT0, _color_texture_id, 0);

        glCreateRenderbuffers(1, &_depth_stencil_renderbuffer_id);
        glNamedRenderbufferStorage(
            _depth_stencil_renderbuffer_id,
            GL_DEPTH24_STENCIL8,
            static_cast<GLsizei>(resolution.width),
            static_cast<GLsizei>(resolution.height));
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

        _resolution = resolution;
    }

    void OpenGlFrameBuffer::set_size(const Size& size)
    {
        set_resolution(size);
    }

    void OpenGlFrameBuffer::preset(
        const uint32 destination_framebuffer_id,
        const Size& destination_size,
        const OpenGlFrameBufferPresentMode mode) const
    {
        TBX_ASSERT(
            _framebuffer_id != 0,
            "OpenGL rendering: preset requires a valid source framebuffer.");

        const auto source_size = get_resolution();
        TBX_ASSERT(
            source_size.width > 0 && source_size.height > 0,
            "OpenGL rendering: preset requires non-zero source framebuffer size.");
        TBX_ASSERT(
            destination_size.width > 0 && destination_size.height > 0,
            "OpenGL rendering: preset requires non-zero destination framebuffer size.");

        auto destination_x = GLint {};
        auto destination_y = GLint {};
        auto destination_width = GLint {};
        auto destination_height = GLint {};
        get_preset_destination_bounds(
            source_size,
            destination_size,
            mode,
            destination_x,
            destination_y,
            destination_width,
            destination_height);

        const auto source_width = static_cast<GLint>(source_size.width);
        const auto source_height = static_cast<GLint>(source_size.height);
        const auto viewport_width = static_cast<GLint>(destination_size.width);
        const auto viewport_height = static_cast<GLint>(destination_size.height);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, _framebuffer_id);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, destination_framebuffer_id);
        glViewport(0, 0, viewport_width, viewport_height);
        glClear(GL_COLOR_BUFFER_BIT);
        glBlitFramebuffer(
            0,
            0,
            source_width,
            source_height,
            destination_x,
            destination_y,
            destination_x + destination_width,
            destination_y + destination_height,
            GL_COLOR_BUFFER_BIT,
            GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
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
