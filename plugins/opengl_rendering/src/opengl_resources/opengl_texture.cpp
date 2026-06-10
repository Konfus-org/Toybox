#include "opengl_texture.h"
#include <glad/glad.h>

namespace opengl_rendering
{
    static uint32 take_texture_gl_handle(uint32& id) noexcept
    {
        return std::exchange(id, 0U);
    }

    static void release_bindless_handle(GLuint64& handle) noexcept
    {
        if (handle != 0)
            glMakeTextureHandleNonResidentARB(handle);
        handle = 0;
    }

    static bool is_depth_texture_format(const tbx::TextureFormat format)
    {
        return format == tbx::TextureFormat::DEPTH24_STENCIL8
               || format == tbx::TextureFormat::DEPTH32_FLOAT;
    }

    GLenum get_depth_attachment(const tbx::TextureFormat format)
    {
        return format == tbx::TextureFormat::DEPTH24_STENCIL8 ? GL_DEPTH_STENCIL_ATTACHMENT
                                                              : GL_DEPTH_ATTACHMENT;
    }

    GLenum get_texture_internal_format(const tbx::TextureFormat format)
    {
        switch (format)
        {
            case tbx::TextureFormat::RGBA16_FLOAT:
                return GL_RGBA16F;
            case tbx::TextureFormat::RGBA32_FLOAT:
                return GL_RGBA32F;
            case tbx::TextureFormat::R8:
                return GL_R8;
            case tbx::TextureFormat::R16_FLOAT:
                return GL_R16F;
            case tbx::TextureFormat::RG8:
                return GL_RG8;
            case tbx::TextureFormat::RG16_FLOAT:
                return GL_RG16F;
            case tbx::TextureFormat::DEPTH24_STENCIL8:
                return GL_DEPTH24_STENCIL8;
            case tbx::TextureFormat::DEPTH32_FLOAT:
                return GL_DEPTH_COMPONENT32F;
            case tbx::TextureFormat::RGBA8:
            case tbx::TextureFormat::RGBA:
            default:
                return GL_RGBA8;
        }
    }

    GLenum get_texture_upload_format(const tbx::TextureFormat format)
    {
        switch (format)
        {
            case tbx::TextureFormat::DEPTH24_STENCIL8:
                return GL_DEPTH_STENCIL;
            case tbx::TextureFormat::DEPTH32_FLOAT:
                return GL_DEPTH_COMPONENT;
            case tbx::TextureFormat::RGB:
                return GL_RGB;
            case tbx::TextureFormat::R8:
            case tbx::TextureFormat::R16_FLOAT:
                return GL_RED;
            case tbx::TextureFormat::RG8:
            case tbx::TextureFormat::RG16_FLOAT:
                return GL_RG;
            case tbx::TextureFormat::RGBA:
            case tbx::TextureFormat::RGBA8:
            case tbx::TextureFormat::RGBA16_FLOAT:
            case tbx::TextureFormat::RGBA32_FLOAT:
            default:
                return GL_RGBA;
        }
    }

    GLenum get_texture_upload_type(const tbx::TextureFormat format)
    {
        switch (format)
        {
            case tbx::TextureFormat::RGBA16_FLOAT:
            case tbx::TextureFormat::RGBA32_FLOAT:
            case tbx::TextureFormat::R16_FLOAT:
            case tbx::TextureFormat::RG16_FLOAT:
            case tbx::TextureFormat::DEPTH32_FLOAT:
                return GL_FLOAT;
            case tbx::TextureFormat::DEPTH24_STENCIL8:
                return GL_UNSIGNED_INT_24_8;
            case tbx::TextureFormat::RGB:
            case tbx::TextureFormat::RGBA:
            case tbx::TextureFormat::RGBA8:
            case tbx::TextureFormat::R8:
            case tbx::TextureFormat::RG8:
            default:
                return GL_UNSIGNED_BYTE;
        }
    }

    uint64 get_texture_byte_size(const tbx::TextureDesc& desc)
    {
        return static_cast<uint64>(desc.size.width) * static_cast<uint64>(desc.size.height)
               * get_texture_bytes_per_pixel(desc.format);
    }

    uint64 get_texture_bytes_per_pixel(const tbx::TextureFormat format)
    {
        switch (format)
        {
            case tbx::TextureFormat::RGBA16_FLOAT:
                return 8U;
            case tbx::TextureFormat::RGBA32_FLOAT:
                return 16U;
            case tbx::TextureFormat::RGB:
                return 3U;
            case tbx::TextureFormat::R8:
                return 1U;
            case tbx::TextureFormat::R16_FLOAT:
            case tbx::TextureFormat::RG8:
                return 2U;
            case tbx::TextureFormat::RGBA:
            case tbx::TextureFormat::RGBA8:
            case tbx::TextureFormat::RG16_FLOAT:
            case tbx::TextureFormat::DEPTH24_STENCIL8:
            case tbx::TextureFormat::DEPTH32_FLOAT:
            default:
                return 4U;
        }
    }

    OpenGlTexture::OpenGlTexture(const tbx::TextureDesc& desc, const void* data)
        : _array_layer_count(std::max(desc.array_layer_count, 1U))
    {
        const GLsizei width = static_cast<GLsizei>(desc.size.width);
        const GLsizei height = static_cast<GLsizei>(desc.size.height);
        const GLsizei levels = static_cast<GLsizei>(std::max(desc.mip_count, 1U));
        const GLsizei layer_count = static_cast<GLsizei>(_array_layer_count);
        const bool is_array_texture = _array_layer_count > 1U;
        const GLenum texture_target = is_array_texture ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D;

        glCreateTextures(texture_target, 1, &_texture_id);
        if (is_array_texture)
        {
            glTextureStorage3D(
                _texture_id,
                levels,
                get_texture_internal_format(desc.format),
                width,
                height,
                layer_count);
        }
        else
        {
            glTextureStorage2D(
                _texture_id,
                levels,
                get_texture_internal_format(desc.format),
                width,
                height);
        }

        if (data != nullptr)
        {
            if (is_array_texture)
            {
                glTextureSubImage3D(
                    _texture_id,
                    0,
                    0,
                    0,
                    0,
                    width,
                    height,
                    layer_count,
                    get_texture_upload_format(desc.format),
                    get_texture_upload_type(desc.format),
                    data);
            }
            else
            {
                glTextureSubImage2D(
                    _texture_id,
                    0,
                    0,
                    0,
                    width,
                    height,
                    get_texture_upload_format(desc.format),
                    get_texture_upload_type(desc.format),
                    data);
            }
        }

        const bool is_depth_format = is_depth_texture_format(desc.format);
        const GLint sampled_filter = desc.is_linear_filtering_enabled ? GL_LINEAR : GL_NEAREST;
        const GLint filter = desc.is_depth_comparison_enabled ? GL_LINEAR
                                                              : (is_depth_format ? GL_NEAREST
                                                                                 : sampled_filter);
        glTextureParameteri(_texture_id, GL_TEXTURE_MIN_FILTER, filter);
        glTextureParameteri(_texture_id, GL_TEXTURE_MAG_FILTER, filter);
        glTextureParameteri(_texture_id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(_texture_id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        if (is_array_texture)
            glTextureParameteri(_texture_id, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        if (is_depth_format)
        {
            glTextureParameteri(
                _texture_id,
                GL_TEXTURE_COMPARE_MODE,
                desc.is_depth_comparison_enabled ? GL_COMPARE_REF_TO_TEXTURE : GL_NONE);
            glTextureParameteri(_texture_id, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        }

        if (desc.mip_count > 1U)
            glGenerateTextureMipmap(_texture_id);
    }

    OpenGlTexture::OpenGlTexture(OpenGlTexture&& other) noexcept
        : _texture_id(take_texture_gl_handle(other._texture_id))
        , _array_layer_count(other._array_layer_count)
        , _bindless_handle(std::exchange(other._bindless_handle, 0))
    {
        other._array_layer_count = 1U;
    }

    OpenGlTexture& OpenGlTexture::operator=(OpenGlTexture&& other) noexcept
    {
        if (this == &other)
            return *this;

        release_bindless_handle(_bindless_handle);
        if (_texture_id != 0)
            glDeleteTextures(1, &_texture_id);

        _texture_id = take_texture_gl_handle(other._texture_id);
        _array_layer_count = other._array_layer_count;
        _bindless_handle = std::exchange(other._bindless_handle, 0);
        other._array_layer_count = 1U;
        return *this;
    }

    OpenGlTexture::~OpenGlTexture() noexcept
    {
        release_bindless_handle(_bindless_handle);
        if (_texture_id != 0)
        {
            glDeleteTextures(1, &_texture_id);
        }
    }

    GLuint64 OpenGlTexture::get_or_create_bindless_handle()
    {
        if (_bindless_handle != 0)
            return _bindless_handle;
        if (_texture_id == 0)
            return 0;

        const GLuint64 handle = glGetTextureHandleARB(_texture_id);
        if (handle == 0)
            return 0;

        // Creating a handle makes the texture immutable; making it resident lets shaders sample it
        // by handle without binding a sampler unit.
        glMakeTextureHandleResidentARB(handle);
        _bindless_handle = handle;
        return _bindless_handle;
    }

    void OpenGlTexture::bind_slot(const uint32 slot) const
    {
        glBindTextureUnit(slot, _texture_id);
    }

    void OpenGlTexture::bind()
    {
        bind_slot(0U);
    }

    void OpenGlTexture::unbind()
    {
        glBindTextureUnit(0U, 0U);
    }

    uint32 OpenGlTexture::get_texture_id() const
    {
        return _texture_id;
    }

    uint32 OpenGlTexture::get_array_layer_count() const
    {
        return _array_layer_count;
    }

    void OpenGlTexture::update(
        const tbx::TextureUpdateDesc& desc,
        const GLenum upload_format,
        const GLenum upload_type,
        const void* data) const
    {
        if (_array_layer_count > 1U)
        {
            glTextureSubImage3D(
                _texture_id,
                static_cast<GLint>(desc.mip_level),
                static_cast<GLint>(desc.x),
                static_cast<GLint>(desc.y),
                static_cast<GLint>(desc.array_layer),
                static_cast<GLsizei>(desc.width),
                static_cast<GLsizei>(desc.height),
                1,
                upload_format,
                upload_type,
                data);
            return;
        }

        glTextureSubImage2D(
            _texture_id,
            static_cast<GLint>(desc.mip_level),
            static_cast<GLint>(desc.x),
            static_cast<GLint>(desc.y),
            static_cast<GLsizei>(desc.width),
            static_cast<GLsizei>(desc.height),
            upload_format,
            upload_type,
            data);
    }
}
