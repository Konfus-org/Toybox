#include "OpenGLTexture.h"
#include <Tbx/Debug/Asserts.h>
#include <glad/glad.h>

namespace Tbx::Plugins::OpenGLRendering
{
    /////// Helpers ///////////////////////////////////

    struct GlTextureFormat
    {
        GLenum InternalFormat = 0;
        GLenum DataFormat = 0;
    };

    int TbxTexFilterToGLTexFilter(const Texture& tex)
    {
        if (tex.Filter == TextureFilter::Nearest)
        {
            return GL_NEAREST;
        }
        else if (tex.Filter == TextureFilter::Linear)
        {
            return GL_LINEAR;
        }
        else
        {
            TBX_ASSERT(false, "Unsupported texture filter type!");
            return GL_LINEAR;
        }
    }

    int TbxTexWrapToGLTexWrap(const Texture& tex)
    {
        if (tex.Wrap == TextureWrap::Repeat)
        {
            return GL_REPEAT;
        }
        else if (tex.Wrap == TextureWrap::MirroredRepeat)
        {
            return GL_MIRRORED_REPEAT;
        }
        else if (tex.Wrap == TextureWrap::ClampToEdge)
        {
            return GL_CLAMP_TO_EDGE;
        }
        else
        {
            TBX_ASSERT(false, "Unsupported texture wrap type!");
            return GL_REPEAT;
        }
    }

    GlTextureFormat TbxTexFormatToGLTexFormat(const Texture& tex)
    {
        const auto& format = tex.Format;
        if (format == TextureFormat::RGBA)
        {
            return { GL_RGBA8, GL_RGBA };
        }
        else if (format == TextureFormat::RGB)
        {
            return { GL_RGB8, GL_RGB };
        }
        else
        {
            return { GL_RGB8, GL_RGB };
            TBX_ASSERT(false, "Unsupported texture format.");
        }
    }

    /////// OpenGLTexture ///////////////////////////////////

    OpenGLTexture::OpenGLTexture(const Texture& tex)
    {
        // Generate texture
        auto id = static_cast<uint32>(RenderId);
        glCreateTextures(GL_TEXTURE_2D, 1, &id);
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        RenderId = id;

        // Convert tbx texture to OpenGL texture
        GlTextureFormat format = TbxTexFormatToGLTexFormat(tex);
        auto filtering = TbxTexFilterToGLTexFilter(tex);
        auto wrapping = TbxTexWrapToGLTexWrap(tex);
        glTextureParameteri(RenderId, GL_TEXTURE_MIN_FILTER, filtering);
        glTextureParameteri(RenderId, GL_TEXTURE_MAG_FILTER, filtering);
        glTextureParameteri(RenderId, GL_TEXTURE_WRAP_S, wrapping);
        glTextureParameteri(RenderId, GL_TEXTURE_WRAP_T, wrapping);

        // Upload texture data to GPU
        glTexImage2D(GL_TEXTURE_2D, 0, format.InternalFormat, tex.Resolution.Width, tex.Resolution.Height, 0, format.DataFormat, GL_UNSIGNED_BYTE, tex.Pixels.data());
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    OpenGLTexture::~OpenGLTexture()
    {
        auto id = static_cast<uint32>(RenderId);
        glDeleteTextures(1, &id);
    }

    void OpenGLTexture::SetSlot(uint32 slot)
    {
        _slot = slot;
    }

    void OpenGLTexture::Activate()
    {
        glBindTextureUnit(_slot, RenderId);
    }

    void OpenGLTexture::Release()
    {
        glBindTextureUnit(_slot, 0);
    }
}