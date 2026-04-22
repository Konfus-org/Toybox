#include "opengl_sampler.h"
#include <utility>

namespace opengl_rendering
{
    static GLuint take_gl_handle(GLuint& handle) noexcept
    {
        return std::exchange(handle, 0U);
    }

    OpenGlSampler::OpenGlSampler(const tbx::GraphicsSamplerDesc& desc)
    {
        glCreateSamplers(1, &_sampler_id);

        const GLint min_filter = desc.is_linear_filtering_enabled
                                     ? (desc.is_mipmapping_enabled ? GL_LINEAR_MIPMAP_LINEAR
                                                                   : GL_LINEAR)
                                     : (desc.is_mipmapping_enabled ? GL_NEAREST_MIPMAP_NEAREST
                                                                   : GL_NEAREST);
        const GLint mag_filter = desc.is_linear_filtering_enabled ? GL_LINEAR : GL_NEAREST;
        const GLint wrap = desc.is_repeating ? GL_REPEAT : GL_CLAMP_TO_EDGE;

        glSamplerParameteri(_sampler_id, GL_TEXTURE_MIN_FILTER, min_filter);
        glSamplerParameteri(_sampler_id, GL_TEXTURE_MAG_FILTER, mag_filter);
        glSamplerParameteri(_sampler_id, GL_TEXTURE_WRAP_S, wrap);
        glSamplerParameteri(_sampler_id, GL_TEXTURE_WRAP_T, wrap);
    }

    OpenGlSampler::~OpenGlSampler() noexcept
    {
        if (_sampler_id != 0U)
            glDeleteSamplers(1, &_sampler_id);
    }

    OpenGlSampler::OpenGlSampler(OpenGlSampler&& other) noexcept
        : _sampler_id(take_gl_handle(other._sampler_id))
    {
    }

    OpenGlSampler& OpenGlSampler::operator=(OpenGlSampler&& other) noexcept
    {
        if (this == &other)
            return *this;

        if (_sampler_id != 0U)
            glDeleteSamplers(1, &_sampler_id);

        _sampler_id = take_gl_handle(other._sampler_id);
        return *this;
    }

    void OpenGlSampler::bind()
    {
        bind_slot(0U);
    }

    void OpenGlSampler::bind_slot(const uint32 slot) const
    {
        glBindSampler(slot, _sampler_id);
    }

    GLuint OpenGlSampler::get_sampler_id() const
    {
        return _sampler_id;
    }

    void OpenGlSampler::unbind()
    {
        glBindSampler(0U, 0U);
    }
}
