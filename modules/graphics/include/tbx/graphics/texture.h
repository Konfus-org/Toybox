#pragma once
#include "tbx/math/size.h"
#include "tbx/tbx_api.h"
#include <vector>

namespace tbx
{
    using Pixel = unsigned char;

    enum class TBX_API TextureFilter
    {
        Nearest,
        Linear
    };

    enum class TBX_API TextureWrap
    {
        ClampToEdge,
        MirroredRepeat,
        Repeat
    };

    enum class TBX_API TextureFormat
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
        TextureWrap wrap = TextureWrap::Repeat;
        TextureFilter filter = TextureFilter::Nearest;
        TextureFormat format = TextureFormat::RGB;
        std::vector<Pixel> pixels = {255, 255, 255};
    };
}
