#include "opengl_texture.h"
#include "internal/opengl_texture_internal.h"
#include "opengl_bindless.h"
#include "opengl_utils.h"
#include "tbx/systems/debugging/macros.h"
#include <algorithm>
#include <glad/glad.h>
#include <utility>
namespace opengl_rendering
{
    OpenGlTexture::OpenGlTexture(const tbx::Texture& texture)
        : _array_layer_count(1U)
    {
        glCreateTextures(GL_TEXTURE_2D, 1, &_texture_id);

        const auto format = internal::to_gl_texture_format(texture.format);
        const auto min_filter = internal::to_gl_texture_min_filter(texture);
        const auto mag_filter = internal::to_gl_texture_mag_filter(texture.filter);
        const auto wrapping = internal::to_gl_texture_wrap(texture.wrap);
        const auto levels = internal::calculate_mipmap_levels(texture);
        const auto internal_format =
            internal::resolve_internal_format(texture, format.internal_format);

        glTextureParameteri(_texture_id, GL_TEXTURE_MIN_FILTER, min_filter);
        glTextureParameteri(_texture_id, GL_TEXTURE_MAG_FILTER, mag_filter);
        glTextureParameteri(_texture_id, GL_TEXTURE_WRAP_S, wrapping);
        glTextureParameteri(_texture_id, GL_TEXTURE_WRAP_T, wrapping);

        glTextureStorage2D(
            _texture_id,
            levels,
            internal_format,
            static_cast<GLsizei>(texture.resolution.width),
            static_cast<GLsizei>(texture.resolution.height));

        glTextureSubImage2D(
            _texture_id,
            0,
            0,
            0,
            static_cast<GLsizei>(texture.resolution.width),
            static_cast<GLsizei>(texture.resolution.height),
            format.data_format,
            GL_UNSIGNED_BYTE,
            texture.pixels.data());
        if (texture.mipmaps == tbx::TextureMipmaps::ENABLED)
            glGenerateTextureMipmap(_texture_id);
    }

    OpenGlTexture::OpenGlTexture(const tbx::GraphicsTextureDesc& desc, const void* data)
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

        const bool is_depth_format = internal::is_depth_texture_format(desc.format);
        glTextureParameteri(
            _texture_id,
            GL_TEXTURE_MIN_FILTER,
            desc.is_depth_comparison_enabled ? GL_LINEAR
                                             : (is_depth_format ? GL_NEAREST : GL_LINEAR));
        glTextureParameteri(
            _texture_id,
            GL_TEXTURE_MAG_FILTER,
            desc.is_depth_comparison_enabled ? GL_LINEAR
                                             : (is_depth_format ? GL_NEAREST : GL_LINEAR));
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
        : _texture_id(internal::take_texture_gl_handle(other._texture_id))
        , _array_layer_count(other._array_layer_count)
        , _bindless_handle(other._bindless_handle)
    {
        other._array_layer_count = 1U;
        other._bindless_handle = 0;
    }

    OpenGlTexture& OpenGlTexture::operator=(OpenGlTexture&& other) noexcept
    {
        if (this == &other)
            return *this;

        if (_texture_id != 0)
            glDeleteTextures(1, &_texture_id);

        if (_bindless_handle != 0)
            release_bindless_handle(_bindless_handle);

        _texture_id = internal::take_texture_gl_handle(other._texture_id);
        _array_layer_count = other._array_layer_count;
        _bindless_handle = other._bindless_handle;
        other._array_layer_count = 1U;
        other._bindless_handle = 0;
        return *this;
    }

    OpenGlTexture::~OpenGlTexture() noexcept
    {
        if (_bindless_handle != 0)
        {
            release_bindless_handle(_bindless_handle);
            _bindless_handle = 0;
        }

        if (_texture_id != 0)
        {
            glDeleteTextures(1, &_texture_id);
        }
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

    uint64 OpenGlTexture::get_bindless_handle() const
    {
        if (_bindless_handle != 0)
            return _bindless_handle;

        auto handle = uint64 {};
        if (!try_make_bindless_handle_resident(_texture_id, handle))
            return 0;

        _bindless_handle = handle;
        return _bindless_handle;
    }

    void OpenGlTexture::update(
        const tbx::GraphicsTextureUpdateDesc& desc,
        const tbx::GraphicsTextureFormat format,
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
                get_texture_upload_format(format),
                get_texture_upload_type(format),
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
            get_texture_upload_format(format),
            get_texture_upload_type(format),
            data);
    }
}
