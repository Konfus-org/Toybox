#include "opengl_gbuffer.h"
#include "tbx/debugging/macros.h"
#include <array>
#include <glad/glad.h>

namespace tbx::plugins
{
    static void allocate_color_texture(
        uint32 framebuffer_id,
        uint32 attachment_index,
        const Size& resolution,
        std::shared_ptr<OpenGlTexture>& out_texture)
    {
        auto settings = OpenGlTextureRuntimeSettings {
            .mode = OpenGlTextureRuntimeMode::HdrColor,
            .resolution = resolution,
            .filter = TextureFilter::NEAREST,
            .wrap = TextureWrap::CLAMP_TO_EDGE,
        };
        out_texture = std::make_shared<OpenGlTexture>(settings);
        glNamedFramebufferTexture(
            framebuffer_id,
            GL_COLOR_ATTACHMENT0 + attachment_index,
            out_texture->get_texture_id(),
            0);
    }

    OpenGlGBuffer::OpenGlGBuffer(const Size& resolution)
    {
        set_resolution(resolution);
    }

    OpenGlGBuffer::~OpenGlGBuffer() noexcept
    {
        _depth_texture.reset();
        _material_texture.reset();
        _normal_texture.reset();
        _albedo_spec_texture.reset();

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

        _depth_texture.reset();
        _material_texture.reset();
        _normal_texture.reset();
        _albedo_spec_texture.reset();

        allocate_color_texture(_framebuffer_id, 0, resolution, _albedo_spec_texture);
        allocate_color_texture(_framebuffer_id, 1, resolution, _normal_texture);
        allocate_color_texture(_framebuffer_id, 2, resolution, _material_texture);

        auto depth_settings = OpenGlTextureRuntimeSettings {
            .mode = OpenGlTextureRuntimeMode::Depth,
            .resolution = resolution,
            .filter = TextureFilter::NEAREST,
            .wrap = TextureWrap::CLAMP_TO_EDGE,
        };
        _depth_texture = std::make_shared<OpenGlTexture>(depth_settings);
        glNamedFramebufferTexture(
            _framebuffer_id,
            GL_DEPTH_ATTACHMENT,
            _depth_texture->get_texture_id(),
            0);

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

    void OpenGlGBuffer::clear(const Color& clear_color) const
    {
        const std::array<float, 4> albedo_spec =
            {clear_color.r, clear_color.g, clear_color.b, clear_color.a};
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
        if (!_albedo_spec_texture)
            return 0;

        return _albedo_spec_texture->get_texture_id();
    }

    std::shared_ptr<OpenGlTexture> OpenGlGBuffer::get_albedo_spec_texture() const
    {
        return _albedo_spec_texture;
    }

    uint32 OpenGlGBuffer::get_normal_texture_id() const
    {
        if (!_normal_texture)
            return 0;

        return _normal_texture->get_texture_id();
    }

    std::shared_ptr<OpenGlTexture> OpenGlGBuffer::get_normal_texture() const
    {
        return _normal_texture;
    }

    uint32 OpenGlGBuffer::get_material_texture_id() const
    {
        if (!_material_texture)
            return 0;

        return _material_texture->get_texture_id();
    }

    std::shared_ptr<OpenGlTexture> OpenGlGBuffer::get_material_texture() const
    {
        return _material_texture;
    }

    uint32 OpenGlGBuffer::get_depth_texture_id() const
    {
        if (!_depth_texture)
            return 0;

        return _depth_texture->get_texture_id();
    }

    std::shared_ptr<OpenGlTexture> OpenGlGBuffer::get_depth_texture() const
    {
        return _depth_texture;
    }
}
