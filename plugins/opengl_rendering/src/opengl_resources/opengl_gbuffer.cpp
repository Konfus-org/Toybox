#include "opengl_gbuffer.h"
#include "tbx/debugging/macros.h"
#include <array>
#include <glad/glad.h>

namespace tbx::plugins
{
    static void delete_texture(uint32& texture_id)
    {
        if (texture_id == 0)
            return;

        glDeleteTextures(1, &texture_id);
        texture_id = 0;
    }

    static void allocate_color_texture(
        uint32 framebuffer_id,
        uint32 attachment_index,
        const Size& resolution,
        uint32& out_texture_id)
    {
        glCreateTextures(GL_TEXTURE_2D, 1, &out_texture_id);
        glTextureParameteri(out_texture_id, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTextureParameteri(out_texture_id, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTextureParameteri(out_texture_id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(out_texture_id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureStorage2D(
            out_texture_id,
            1,
            GL_RGBA16F,
            static_cast<GLsizei>(resolution.width),
            static_cast<GLsizei>(resolution.height));
        glNamedFramebufferTexture(
            framebuffer_id,
            GL_COLOR_ATTACHMENT0 + attachment_index,
            out_texture_id,
            0);
    }

    OpenGlGBuffer::OpenGlGBuffer(const Size& resolution)
    {
        set_resolution(resolution);
    }

    OpenGlGBuffer::~OpenGlGBuffer() noexcept
    {
        delete_texture(_depth_texture_id);
        delete_texture(_material_texture_id);
        delete_texture(_normal_texture_id);
        delete_texture(_albedo_spec_texture_id);

        if (_framebuffer_id != 0)
        {
            glDeleteFramebuffers(1, &_framebuffer_id);
            _framebuffer_id = 0;
        }
    }

    void OpenGlGBuffer::bind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, _framebuffer_id);
    }

    void OpenGlGBuffer::unbind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void OpenGlGBuffer::set_resolution(const Size& resolution)
    {
        TBX_ASSERT(
            resolution.width > 0 && resolution.height > 0,
            "OpenGL rendering: G-buffer resolution must be greater than zero.");

        if (_framebuffer_id == 0)
            glCreateFramebuffers(1, &_framebuffer_id);

        delete_texture(_depth_texture_id);
        delete_texture(_material_texture_id);
        delete_texture(_normal_texture_id);
        delete_texture(_albedo_spec_texture_id);

        allocate_color_texture(_framebuffer_id, 0, resolution, _albedo_spec_texture_id);
        allocate_color_texture(_framebuffer_id, 1, resolution, _normal_texture_id);
        allocate_color_texture(_framebuffer_id, 2, resolution, _material_texture_id);

        glCreateTextures(GL_TEXTURE_2D, 1, &_depth_texture_id);
        glTextureParameteri(_depth_texture_id, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTextureParameteri(_depth_texture_id, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTextureParameteri(_depth_texture_id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(_depth_texture_id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureStorage2D(
            _depth_texture_id,
            1,
            GL_DEPTH_COMPONENT24,
            static_cast<GLsizei>(resolution.width),
            static_cast<GLsizei>(resolution.height));
        glNamedFramebufferTexture(_framebuffer_id, GL_DEPTH_ATTACHMENT, _depth_texture_id, 0);

        const std::array<GLenum, 3> draw_buffers = {
            GL_COLOR_ATTACHMENT0,
            GL_COLOR_ATTACHMENT1,
            GL_COLOR_ATTACHMENT2};
        glNamedFramebufferDrawBuffers(
            _framebuffer_id,
            static_cast<GLsizei>(draw_buffers.size()),
            draw_buffers.data());

        const GLenum status = glCheckNamedFramebufferStatus(_framebuffer_id, GL_FRAMEBUFFER);
        TBX_ASSERT(
            status == GL_FRAMEBUFFER_COMPLETE,
            "OpenGL rendering: G-buffer framebuffer is incomplete (status={}).",
            static_cast<uint32>(status));

        _resolution = resolution;
    }

    void OpenGlGBuffer::clear(const RgbaColor& clear_color) const
    {
        const std::array<float, 4> albedo_spec = {
            clear_color.r,
            clear_color.g,
            clear_color.b,
            clear_color.a};
        const std::array<float, 4> normal = {0.5f, 0.5f, 1.0f, 0.0f};
        const std::array<float, 4> material = {0.0f, 0.0f, 0.0f, 1.0f};
        const float depth = 1.0f;

        glClearNamedFramebufferfv(_framebuffer_id, GL_COLOR, 0, albedo_spec.data());
        glClearNamedFramebufferfv(_framebuffer_id, GL_COLOR, 1, normal.data());
        glClearNamedFramebufferfv(_framebuffer_id, GL_COLOR, 2, material.data());
        glClearNamedFramebufferfv(_framebuffer_id, GL_DEPTH, 0, &depth);
    }

    Size OpenGlGBuffer::get_resolution() const
    {
        return _resolution;
    }

    uint32 OpenGlGBuffer::get_framebuffer_id() const
    {
        return _framebuffer_id;
    }

    uint32 OpenGlGBuffer::get_albedo_spec_texture_id() const
    {
        return _albedo_spec_texture_id;
    }

    uint32 OpenGlGBuffer::get_normal_texture_id() const
    {
        return _normal_texture_id;
    }

    uint32 OpenGlGBuffer::get_material_texture_id() const
    {
        return _material_texture_id;
    }

    uint32 OpenGlGBuffer::get_depth_texture_id() const
    {
        return _depth_texture_id;
    }
}
