#include "OpenGLTexture.h"

namespace OpenGLRendering
{
    OpenGLTexture::OpenGLTexture()
    {
        glCreateTextures(GL_TEXTURE_2D, 1, &_rendererId);

        glTextureParameteri(_rendererId, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(_rendererId, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glTextureParameteri(_rendererId, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTextureParameteri(_rendererId, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }

    OpenGLTexture::~OpenGLTexture()
    {
        glDeleteTextures(1, &_rendererId);
    }

    void OpenGLTexture::SetData(const Tbx::Texture& tex) const
    {
        glTextureStorage2D(_rendererId, 1, GL_RGB8, tex.GetWidth(), tex.GetHeight());
        glTextureSubImage2D(_rendererId, 0, 0, 0, tex.GetWidth(), tex.GetHeight(), GL_RGB, GL_UNSIGNED_BYTE, tex.GetData().get());
    }

    void OpenGLTexture::Bind() const
    {
        glBindTextureUnit(0, _rendererId);
    }

    void OpenGLTexture::Unbind() const
    {
        glBindTextureUnit(0, 0);
    }
}