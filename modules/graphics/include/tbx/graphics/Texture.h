#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Math/Size.h"
#include "Tbx/Ids/Uid.h"
#include <vector>

namespace Tbx
{
    using Pixel = unsigned char;

    enum class TBX_EXPORT TextureFilter
    {
        Nearest,
        Linear
    };

    enum class TBX_EXPORT TextureWrap
    {
        ClampToEdge,
        MirroredRepeat,
        Repeat
    };

    enum class TBX_EXPORT TextureFormat
    {
        RGB,
        RGBA
    };

    struct TBX_EXPORT Texture
    {
        Texture() = default;
        Texture(
            const Size& resolution,
            TextureWrap wrap,
            TextureFilter filter,
            TextureFormat format,
            const std::vector<Pixel>& pixels)
            : Resolution(resolution)
            , Wrap(wrap)
            , Filter(filter)
            , Format(format)
            , Pixels(pixels)
        {
        }

        Size Resolution = { 1, 1 };
        TextureWrap Wrap = TextureWrap::Repeat;
        TextureFilter Filter = TextureFilter::Nearest;
        TextureFormat Format = TextureFormat::RGB;
        std::vector<Pixel> Pixels = { 255, 255, 255 };
        Uid Id = Uid::Generate();
    };
}
