#include "opengl_shadow_map.h"
#include "tbx/debugging/macros.h"

namespace tbx::plugins
{
    OpenGlShadowMap::OpenGlShadowMap(const Size& resolution)
    {
        set_resolution(resolution);
    }

    OpenGlShadowMap::~OpenGlShadowMap() noexcept
    {
        _texture.reset();
    }

    void OpenGlShadowMap::bind() {}

    void OpenGlShadowMap::unbind() {}

    void OpenGlShadowMap::set_resolution(const Size& resolution)
    {
        TBX_ASSERT(
            resolution.width > 0 && resolution.height > 0,
            "OpenGL rendering: shadow-map resolution must be greater than zero.");

        auto settings = OpenGlTextureRuntimeSettings {
            .mode = OpenGlTextureRuntimeMode::Depth,
            .resolution = resolution,
            .filter = TextureFilter::LINEAR,
            .wrap = TextureWrap::CLAMP_TO_EDGE,
            .use_border_color = true,
            .border_color = Vec4(1.0f, 1.0f, 1.0f, 1.0f),
        };
        _texture = std::make_shared<OpenGlTexture>(settings);
        _resolution = resolution;
    }

    Size OpenGlShadowMap::get_resolution() const
    {
        return _resolution;
    }

    uint32 OpenGlShadowMap::get_texture_id() const
    {
        if (_texture == nullptr)
            return 0;

        return _texture->get_texture_id();
    }

    std::shared_ptr<OpenGlTexture> OpenGlShadowMap::get_texture() const
    {
        return _texture;
    }
}
