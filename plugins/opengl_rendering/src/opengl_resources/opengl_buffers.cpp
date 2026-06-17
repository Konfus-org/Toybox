#include "opengl_buffers.h"
#include "tbx/systems/debugging/macros.h"
#include <glad/glad.h>

namespace opengl_rendering
{
    static uint32 take_buffer_gl_handle(uint32& id) noexcept
    {
        return std::exchange(id, 0U);
    }

    bool has_buffer_usage(
        const tbx::BufferUsage value,
        const tbx::BufferUsage usage)
    {
        return (static_cast<uint32>(value) & static_cast<uint32>(usage)) != 0U;
    }

    GLenum to_gl_buffer_target(const tbx::BufferUsage usage)
    {
        if (has_buffer_usage(usage, tbx::BufferUsage::INDEX))
            return GL_ELEMENT_ARRAY_BUFFER;
        if (has_buffer_usage(usage, tbx::BufferUsage::UNIFORM))
            return GL_UNIFORM_BUFFER;
        if (has_buffer_usage(usage, tbx::BufferUsage::STORAGE))
            return GL_SHADER_STORAGE_BUFFER;
        if (has_buffer_usage(usage, tbx::BufferUsage::INDIRECT_ARGS))
            return GL_DRAW_INDIRECT_BUFFER;

        return GL_ARRAY_BUFFER;
    }

    GLenum to_gl_buffer_usage(const tbx::BufferDesc& desc)
    {
        return desc.is_dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW;
    }

    OpenGlGraphicsBuffer::OpenGlGraphicsBuffer(
        const tbx::BufferDesc& desc,
        const void* data,
        const uint64 data_size)
        : _target(to_gl_buffer_target(desc.usage))
    {
        const auto* upload_data = data_size == desc.size ? data : nullptr;
        glCreateBuffers(1, &_buffer_id);
        glNamedBufferData(
            _buffer_id,
            static_cast<GLsizeiptr>(desc.size),
            upload_data,
            to_gl_buffer_usage(desc));
        if (upload_data == nullptr && data != nullptr && data_size > 0U)
            update(data, data_size, 0U);
    }

    OpenGlGraphicsBuffer::~OpenGlGraphicsBuffer() noexcept
    {
        if (_buffer_id != 0U)
            glDeleteBuffers(1, &_buffer_id);
    }

    OpenGlGraphicsBuffer::OpenGlGraphicsBuffer(OpenGlGraphicsBuffer&& other) noexcept
        : _buffer_id(take_buffer_gl_handle(other._buffer_id))
        , _target(other._target)
    {
        other._target = GL_ARRAY_BUFFER;
    }

    OpenGlGraphicsBuffer& OpenGlGraphicsBuffer::operator=(OpenGlGraphicsBuffer&& other) noexcept
    {
        if (this == &other)
            return *this;

        if (_buffer_id != 0U)
            glDeleteBuffers(1, &_buffer_id);

        _buffer_id = take_buffer_gl_handle(other._buffer_id);
        _target = other._target;
        other._target = GL_ARRAY_BUFFER;
        return *this;
    }

    void OpenGlGraphicsBuffer::bind()
    {
        glBindBuffer(_target, _buffer_id);
    }

    void OpenGlGraphicsBuffer::bind_slot(const uint32 slot) const
    {
        glBindBufferBase(_target, slot, _buffer_id);
    }

    GLuint OpenGlGraphicsBuffer::get_buffer_id() const
    {
        return _buffer_id;
    }

    void OpenGlGraphicsBuffer::unbind()
    {
        glBindBuffer(_target, 0U);
    }

    void OpenGlGraphicsBuffer::update(const void* data, const uint64 data_size, const uint64 offset)
        const
    {
        glNamedBufferSubData(
            _buffer_id,
            static_cast<GLintptr>(offset),
            static_cast<GLsizeiptr>(data_size),
            data);
    }

    OpenGlFramebuffer::OpenGlFramebuffer()
    {
        glCreateFramebuffers(1, &_framebuffer_id);
    }

    OpenGlFramebuffer::~OpenGlFramebuffer() noexcept
    {
        if (_framebuffer_id != 0U)
            glDeleteFramebuffers(1, &_framebuffer_id);
    }

    OpenGlFramebuffer::OpenGlFramebuffer(OpenGlFramebuffer&& other) noexcept
        : _framebuffer_id(take_buffer_gl_handle(other._framebuffer_id))
    {
    }

    OpenGlFramebuffer& OpenGlFramebuffer::operator=(OpenGlFramebuffer&& other) noexcept
    {
        if (this == &other)
            return *this;

        if (_framebuffer_id != 0U)
            glDeleteFramebuffers(1, &_framebuffer_id);

        _framebuffer_id = take_buffer_gl_handle(other._framebuffer_id);
        return *this;
    }

    void OpenGlFramebuffer::attach_color(const uint32 index, const OpenGlTexture& texture) const
    {
        glNamedFramebufferTexture(
            _framebuffer_id,
            GL_COLOR_ATTACHMENT0 + index,
            texture.get_texture_id(),
            0);
    }

    void OpenGlFramebuffer::attach_depth_stencil(
        const OpenGlTexture& texture,
        const GLenum attachment,
        const int32 layer) const
    {
        if (layer >= 0)
        {
            glNamedFramebufferTextureLayer(
                _framebuffer_id,
                attachment,
                texture.get_texture_id(),
                0,
                layer);
            return;
        }

        glNamedFramebufferTexture(_framebuffer_id, attachment, texture.get_texture_id(), 0);
    }

    void OpenGlFramebuffer::bind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, _framebuffer_id);
    }

    bool OpenGlFramebuffer::is_complete() const
    {
        return glCheckNamedFramebufferStatus(_framebuffer_id, GL_FRAMEBUFFER)
               == GL_FRAMEBUFFER_COMPLETE;
    }

    void OpenGlFramebuffer::set_draw_buffers(const uint32 color_target_count) const
    {
        constexpr auto max_color_target_count = uint32(16U);
        if (color_target_count == 0U)
        {
            glNamedFramebufferDrawBuffer(_framebuffer_id, GL_NONE);
            glNamedFramebufferReadBuffer(_framebuffer_id, GL_NONE);
            return;
        }

        const uint32 draw_buffer_count = color_target_count > max_color_target_count
                                             ? max_color_target_count
                                             : color_target_count;
        TBX_ASSERT(
            draw_buffer_count == color_target_count,
            "OpenGL rendering: render pass requested more color targets than supported.");

        auto draw_buffers = std::array<GLenum, max_color_target_count> {};
        for (uint32 index = 0U; index < draw_buffer_count; ++index)
            draw_buffers[static_cast<size>(index)] = GL_COLOR_ATTACHMENT0 + index;

        glNamedFramebufferDrawBuffers(
            _framebuffer_id,
            static_cast<GLsizei>(draw_buffer_count),
            draw_buffers.data());
    }

    void OpenGlFramebuffer::unbind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0U);
    }
}
