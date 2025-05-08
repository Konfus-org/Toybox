#include "OpenGLTexture.h"

namespace OpenGLRendering
{
    OpenGLTexture::OpenGLTexture()
    {
        glCreateTextures(GL_TEXTURE_2D, 1, &_rendererId);
    }

    OpenGLTexture::~OpenGLTexture()
    {
        glDeleteTextures(1, &_rendererId);
    }

    void OpenGLTexture::SetData(const Tbx::Texture& tex, const Tbx::uint& slot)
    {
        _associatedAssetId = tex.Id;
        _slot = slot;

        // Setup texture parameters
        auto filtering = 0;
        if (tex.GetFilter() == Tbx::TextureFilter::Nearest)
            filtering = GL_NEAREST;
        else if (tex.GetFilter() == Tbx::TextureFilter::Linear)
            filtering = GL_LINEAR;

        auto wrapping = 0;
        if (tex.GetWrap() == Tbx::TextureWrap::Repeat)
            wrapping = GL_REPEAT;
        else if (tex.GetWrap() == Tbx::TextureWrap::MirroredRepeat)
            wrapping = GL_MIRRORED_REPEAT;
        else if (tex.GetWrap() == Tbx::TextureWrap::ClampToEdge)
            wrapping = GL_CLAMP_TO_EDGE;

        glTextureParameteri(_rendererId, GL_TEXTURE_MIN_FILTER, filtering);
        glTextureParameteri(_rendererId, GL_TEXTURE_MAG_FILTER, filtering);
        glTextureParameteri(_rendererId, GL_TEXTURE_WRAP_S, wrapping);
        glTextureParameteri(_rendererId, GL_TEXTURE_WRAP_T, wrapping);

        // Upload texture data to GPU
        glTextureStorage2D(_rendererId, 1, GL_RGB8, tex.GetWidth(), tex.GetHeight());
        glTextureSubImage2D(_rendererId, 0, 0, 0, tex.GetWidth(), tex.GetHeight(), GL_RGB, GL_UNSIGNED_BYTE, tex.GetData().get());
    }

    void OpenGLTexture::Bind() const
    {
        glBindTextureUnit(_slot, _rendererId);
    }

    void OpenGLTexture::Unbind() const
    {
        glBindTextureUnit(_slot, 0);
    }
}