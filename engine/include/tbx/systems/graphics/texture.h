#pragma once
#include "tbx/systems/math/size.h"
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

    enum class TextureMipmaps
    {
        DISABLED,
        ENABLED
    };

    enum class TextureCompression
    {
        DISABLED,
        AUTO
    };

    /// @brief
    /// Purpose: Stores texture sampling, surface settings, and pixel data.
    /// @details
    /// Ownership: Owns texture pixel data by value.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.
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
        Texture(
            const Size& resolution,
            TextureWrap wrap,
            TextureFilter filter,
            TextureFormat format,
            TextureMipmaps mipmaps,
            TextureCompression compression,
            const std::vector<Pixel>& pixels)
            : resolution(resolution)
            , wrap(wrap)
            , filter(filter)
            , format(format)
            , mipmaps(mipmaps)
            , compression(compression)
            , pixels(pixels)
        {
        }

        bool operator==(const Texture& other) const
        {
            return resolution.width == other.resolution.width
                   && resolution.height == other.resolution.height && wrap == other.wrap
                   && filter == other.filter && format == other.format && mipmaps == other.mipmaps
                   && compression == other.compression && pixels == other.pixels;
        }

        Size resolution = {1, 1};
        TextureWrap wrap = TextureWrap::REPEAT;
        TextureFilter filter = TextureFilter::LINEAR;
        TextureFormat format = TextureFormat::RGB;
        TextureMipmaps mipmaps = TextureMipmaps::ENABLED;
        TextureCompression compression = TextureCompression::DISABLED;
        std::vector<Pixel> pixels = {255, 255, 255};
    };
}
