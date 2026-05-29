#pragma once
#include "tbx/systems/assets/serialization.h"
#include "tbx/types/assets/asset.h"
#include "tbx/types/assets/texture.generated.h"
#include "tbx/types/size.h"

namespace tbx
{
    using Pixel = unsigned char;

    [[serializable]];
    enum class TextureFilter
    {
        NEAREST [[name("nearest")]],
        LINEAR [[name("linear")]]
    };

    [[serializable]];
    enum class TextureWrap
    {
        CLAMP_TO_EDGE [[name("clamp_to_edge")]],
        MIRRORED_REPEAT [[name("mirrored_repeat")]],
        REPEAT [[name("repeat")]]
    };

    [[serializable]];
    enum class TextureFormat
    {
        RGB [[name("rgb")]],
        RGBA [[name("rgba")]]
    };

    [[serializable]];
    enum class TextureMipmaps
    {
        DISABLED [[name("disabled")]],
        ENABLED [[name("enabled")]]
    };

    [[serializable]];
    enum class TextureCompression
    {
        DISABLED [[name("disabled")]],
        AUTO [[name("auto")]]
    };

    /// @brief
    /// Purpose: Stores texture sampling, surface settings, and pixel data.
    /// @details
    /// Ownership: Owns texture pixel data by value.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.
    [[serializable]];
    [[version(1U)]];
    struct TBX_API Texture : Asset
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
        [[meta]]
        TextureWrap wrap = TextureWrap::REPEAT;

        [[meta]]
        TextureFilter filter = TextureFilter::LINEAR;

        [[meta]]
        TextureFormat format = TextureFormat::RGB;

        [[meta]]
        TextureMipmaps mipmaps = TextureMipmaps::ENABLED;

        [[meta]]
        TextureCompression compression = TextureCompression::DISABLED;
        std::vector<Pixel> pixels = {255, 255, 255};
    };

    struct TBX_API RenderTexture : Texture
    {
    };

}
