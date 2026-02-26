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

        _color_texture.reset();

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
        if (!_color_texture)
            return 0;

        return _color_texture->get_texture_id();
    }

    std::shared_ptr<OpenGlTexture> OpenGlFrameBuffer::get_color_texture() const
    {
        return _color_texture;
    }

    void OpenGlFrameBuffer::set_filtering(const TextureFilter filtering)
    {
        _filtering = filtering;
        if (!_color_texture)
            return;

        const auto gl_filter = to_gl_texture_filter(_filtering);
        const auto texture_id = _color_texture->get_texture_id();
        glTextureParameteri(texture_id, GL_TEXTURE_MIN_FILTER, gl_filter);
        glTextureParameteri(texture_id, GL_TEXTURE_MAG_FILTER, gl_filter);
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

        _color_texture.reset();
        if (_depth_stencil_renderbuffer_id != 0)
        {
            glDeleteRenderbuffers(1, &_depth_stencil_renderbuffer_id);
        }

        auto color_texture_settings = OpenGlTextureRuntimeSettings {
            .mode = OpenGlTextureRuntimeMode::Color,
            .resolution = resolution,
            .filter = _filtering,
            .wrap = TextureWrap::CLAMP_TO_EDGE,
        };
        _color_texture = std::make_shared<OpenGlTexture>(color_texture_settings);
        const auto color_texture_id = _color_texture->get_texture_id();
        glNamedFramebufferTexture(_framebuffer_id, GL_COLOR_ATTACHMENT0, color_texture_id, 0);

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
            to_gl_texture_filter(_filtering));
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

    OpenGlStorageBuffer::OpenGlStorageBuffer()
    {
        glCreateBuffers(1, &_buffer_id);
    }

    OpenGlStorageBuffer::~OpenGlStorageBuffer() noexcept
    {
        if (_buffer_id != 0)
            glDeleteBuffers(1, &_buffer_id);
    }

    void OpenGlStorageBuffer::ensure_capacity(size_t byte_count)
    {
        if (_buffer_id == 0)
            return;
        if (_capacity_bytes >= byte_count)
            return;

        _capacity_bytes = byte_count;
        glNamedBufferData(
            _buffer_id,
            static_cast<GLsizeiptr>(_capacity_bytes),
            nullptr,
            GL_DYNAMIC_DRAW);
    }

    void OpenGlStorageBuffer::upload(const void* data, size_t byte_count, size_t byte_offset)
    {
        if (_buffer_id == 0 || data == nullptr || byte_count == 0)
            return;

        const size_t required_bytes = byte_offset + byte_count;
        ensure_capacity(required_bytes);
        glNamedBufferSubData(
            _buffer_id,
            static_cast<GLintptr>(byte_offset),
            static_cast<GLsizeiptr>(byte_count),
            data);
    }

    void OpenGlStorageBuffer::clear_u32(const uint32 value) const
    {
        if (_buffer_id == 0 || _capacity_bytes == 0U)
            return;

        glClearNamedBufferData(
            _buffer_id,
            GL_R32UI,
            GL_RED_INTEGER,
            GL_UNSIGNED_INT,
            &value);
    }

    void OpenGlStorageBuffer::bind_base(const uint32 binding_point) const
    {
        if (_buffer_id == 0)
            return;

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding_point, _buffer_id);
    }

    uint32 OpenGlStorageBuffer::get_buffer_id() const
    {
        return _buffer_id;
    }

    size_t OpenGlStorageBuffer::get_capacity_bytes() const
    {
        return _capacity_bytes;
    }

    void OpenGlStorageBuffer::bind()
    {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, _buffer_id);
    }

    void OpenGlStorageBuffer::unbind()
    {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }
}
