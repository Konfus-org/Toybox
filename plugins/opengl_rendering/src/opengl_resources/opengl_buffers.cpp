#include "opengl_buffers.h"
#include "tbx/debugging/macros.h"
#include <array>
#include <glad/glad.h>
#include <utility>
#include <variant>

namespace opengl_rendering
{
    static uint32 take_gl_handle(uint32& id) noexcept
    {
        return std::exchange(id, 0);
    }

    static constexpr auto GeometryPassDrawBuffers = std::array<GLenum, 7U> {
        GL_NONE,
        GL_COLOR_ATTACHMENT1,
        GL_COLOR_ATTACHMENT2,
        GL_COLOR_ATTACHMENT3,
        GL_COLOR_ATTACHMENT4,
        GL_COLOR_ATTACHMENT5,
        GL_COLOR_ATTACHMENT6,
    };

    static void add_attribute(
        const uint32 vertex_array_id,
        const uint32 index,
        const uint32 size,
        const uint32 type,
        const uint32 offset,
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
        if (std::holds_alternative<tbx::Vec4>(type))
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

    static GLenum get_stage_attachment(const tbx::RenderStage render_stage)
    {
        switch (render_stage)
        {
            case tbx::RenderStage::FINAL_COLOR:
                return GL_COLOR_ATTACHMENT0;
            case tbx::RenderStage::GEOMETRY_PREVIEW_COLOR:
                return GL_COLOR_ATTACHMENT1;
            case tbx::RenderStage::ALBEDO:
                return GL_COLOR_ATTACHMENT2;
            case tbx::RenderStage::NORMAL:
                return GL_COLOR_ATTACHMENT3;
            case tbx::RenderStage::DEPTH_PREVIEW:
                return GL_COLOR_ATTACHMENT4;
            default:
                return GL_COLOR_ATTACHMENT0;
        }
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
        const uint32 vertex_array_id,
        const tbx::VertexBuffer& buffer)
    {
        _count = static_cast<uint32>(buffer.vertices.size());
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

        uint32 index = 0;
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

    uint32 OpenGlVertexBuffer::get_count() const
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
        const uint32 vertex_array_id,
        const tbx::IndexBuffer& buffer)
    {
        _count = static_cast<uint32>(buffer.size());
        glNamedBufferData(
            _buffer_id,
            static_cast<GLsizeiptr>(_count * sizeof(uint32)),
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

    uint32 OpenGlIndexBuffer::get_count() const
    {
        return _count;
    }

    OpenGlGBuffer::~OpenGlGBuffer() noexcept
    {
        destroy();
    }

    void OpenGlGBuffer::resize(const tbx::Size& size)
    {
        if (size.width == 0U || size.height == 0U)
            return;

        if (_size.width == size.width && _size.height == size.height
            && _geometry_framebuffer != 0U && _final_color_framebuffer != 0U)
            return;

        destroy();
        _size = size;

        glCreateFramebuffers(1, &_geometry_framebuffer);
        glCreateFramebuffers(1, &_final_color_framebuffer);

        _final_color = create_color_attachment(GL_RGBA8, _size.width, _size.height);
        _geometry_preview_color = create_color_attachment(GL_RGBA8, _size.width, _size.height);
        _albedo = create_color_attachment(GL_RGBA8, _size.width, _size.height);
        _normal = create_color_attachment(GL_RGBA16F, _size.width, _size.height);
        _depth_preview = create_color_attachment(GL_RGBA8, _size.width, _size.height);
        _emissive = create_color_attachment(GL_RGBA16F, _size.width, _size.height);
        _material = create_color_attachment(GL_RGBA16F, _size.width, _size.height);
        _depth = create_depth_attachment(_size.width, _size.height);

        glNamedFramebufferTexture(
            _geometry_framebuffer,
            GL_COLOR_ATTACHMENT0,
            _final_color,
            0);
        glNamedFramebufferTexture(
            _geometry_framebuffer,
            GL_COLOR_ATTACHMENT1,
            _geometry_preview_color,
            0);
        glNamedFramebufferTexture(_geometry_framebuffer, GL_COLOR_ATTACHMENT2, _albedo, 0);
        glNamedFramebufferTexture(_geometry_framebuffer, GL_COLOR_ATTACHMENT3, _normal, 0);
        glNamedFramebufferTexture(
            _geometry_framebuffer,
            GL_COLOR_ATTACHMENT4,
            _depth_preview,
            0);
        glNamedFramebufferTexture(_geometry_framebuffer, GL_COLOR_ATTACHMENT5, _emissive, 0);
        glNamedFramebufferTexture(_geometry_framebuffer, GL_COLOR_ATTACHMENT6, _material, 0);
        glNamedFramebufferTexture(_geometry_framebuffer, GL_DEPTH_ATTACHMENT, _depth, 0);
        glNamedFramebufferDrawBuffers(
            _geometry_framebuffer,
            static_cast<GLsizei>(GeometryPassDrawBuffers.size()),
            GeometryPassDrawBuffers.data());

        glNamedFramebufferTexture(
            _final_color_framebuffer,
            GL_COLOR_ATTACHMENT0,
            _final_color,
            0);
        glNamedFramebufferTexture(_final_color_framebuffer, GL_DEPTH_ATTACHMENT, _depth, 0);
        glNamedFramebufferDrawBuffer(_final_color_framebuffer, GL_COLOR_ATTACHMENT0);

        const auto geometry_status =
            glCheckNamedFramebufferStatus(_geometry_framebuffer, GL_FRAMEBUFFER);
        TBX_ASSERT(
            geometry_status == GL_FRAMEBUFFER_COMPLETE,
            "OpenGL geometry g-buffer framebuffer is incomplete.");

        const auto final_color_status =
            glCheckNamedFramebufferStatus(_final_color_framebuffer, GL_FRAMEBUFFER);
        TBX_ASSERT(
            final_color_status == GL_FRAMEBUFFER_COMPLETE,
            "OpenGL final-color framebuffer is incomplete.");
    }

    void OpenGlGBuffer::prepare_geometry_pass() const
    {
        glBindFramebuffer(GL_FRAMEBUFFER, _geometry_framebuffer);
    }

    void OpenGlGBuffer::bind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, _final_color_framebuffer);
    }

    void OpenGlGBuffer::unbind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void OpenGlGBuffer::present(const tbx::RenderStage render_stage, const tbx::Size& viewport_size)
        const
    {
        if (_geometry_framebuffer == 0U || viewport_size.width == 0U || viewport_size.height == 0U)
            return;

        glBindFramebuffer(GL_READ_FRAMEBUFFER, _geometry_framebuffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glReadBuffer(get_stage_attachment(render_stage));
        glBlitFramebuffer(
            0,
            0,
            static_cast<GLint>(_size.width),
            static_cast<GLint>(_size.height),
            0,
            0,
            static_cast<GLint>(viewport_size.width),
            static_cast<GLint>(viewport_size.height),
            GL_COLOR_BUFFER_BIT,
            GL_NEAREST);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    }

    GLuint OpenGlGBuffer::get_albedo_texture() const
    {
        return _albedo;
    }

    GLuint OpenGlGBuffer::get_normal_texture() const
    {
        return _normal;
    }

    GLuint OpenGlGBuffer::get_emissive_texture() const
    {
        return _emissive;
    }

    GLuint OpenGlGBuffer::get_material_texture() const
    {
        return _material;
    }

    GLuint OpenGlGBuffer::get_depth_texture() const
    {
        return _depth;
    }

    GLuint OpenGlGBuffer::create_color_attachment(
        const GLenum internal_format,
        const uint32 width,
        const uint32 height)
    {
        auto texture_id = GLuint {0U};
        glCreateTextures(GL_TEXTURE_2D, 1, &texture_id);
        glTextureStorage2D(
            texture_id,
            1,
            internal_format,
            static_cast<GLsizei>(width),
            static_cast<GLsizei>(height));
        glTextureParameteri(texture_id, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTextureParameteri(texture_id, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTextureParameteri(texture_id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(texture_id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        return texture_id;
    }

    GLuint OpenGlGBuffer::create_depth_attachment(const uint32 width, const uint32 height)
    {
        auto texture_id = GLuint {0U};
        glCreateTextures(GL_TEXTURE_2D, 1, &texture_id);
        glTextureStorage2D(
            texture_id,
            1,
            GL_DEPTH_COMPONENT24,
            static_cast<GLsizei>(width),
            static_cast<GLsizei>(height));
        glTextureParameteri(texture_id, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTextureParameteri(texture_id, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTextureParameteri(texture_id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(texture_id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        return texture_id;
    }

    void OpenGlGBuffer::delete_texture(GLuint& texture_id)
    {
        if (texture_id == 0U)
            return;

        glDeleteTextures(1, &texture_id);
        texture_id = 0U;
    }

    void OpenGlGBuffer::destroy()
    {
        delete_texture(_final_color);
        delete_texture(_geometry_preview_color);
        delete_texture(_albedo);
        delete_texture(_normal);
        delete_texture(_depth_preview);
        delete_texture(_emissive);
        delete_texture(_material);
        delete_texture(_depth);

        if (_final_color_framebuffer != 0U)
        {
            glDeleteFramebuffers(1, &_final_color_framebuffer);
            _final_color_framebuffer = 0U;
        }

        if (_geometry_framebuffer != 0U)
        {
            glDeleteFramebuffers(1, &_geometry_framebuffer);
            _geometry_framebuffer = 0U;
        }

        _size = {0U, 0U};
    }
}
