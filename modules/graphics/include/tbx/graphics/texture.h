#pragma once
#include "tbx/math/size.h"
#include "tbx/tbx_api.h"
#include <vector>

namespace tbx
{
    using Pixel = unsigned char;

    enum class TextureFilter
    {
        NEAREST,
        LINEAR
    };

    enum class TextureWrap
    {
        CLAMP_TO_EDGE,
        MIRRORED_REPEAT,
        REPEAT
    };

    enum class TextureFormat
    {
        RGB,
        RGBA
    };

    struct TBX_API Texture
    {
        Texture() = default;
        Texture(
            const Size& resolution,
            TextureWrap wrap,
            TextureFilter filter,
            TextureFormat format,
            const std::vector<Pixel>& pixels)
            : resolution(resolution)
            , wrap(wrap)
            , filter(filter)
            , format(format)
            , pixels(pixels)
        {
        }

        Size resolution = {1, 1};
        TextureWrap wrap = TextureWrap::REPEAT;
        TextureFilter filter = TextureFilter::NEAREST;
        TextureFormat format = TextureFormat::RGB;
        std::vector<Pixel> pixels = {255, 255, 255};
    };
}
