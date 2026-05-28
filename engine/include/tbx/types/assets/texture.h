#pragma once
#include "tbx/systems/assets/serialization.h"
#include "tbx/tbx_api.h"
#include "tbx/types/assets/asset.h"
#include "tbx/types/size.h"
#include <vector>

namespace tbx
{
    using Pixel = unsigned char;

    [[tbx::serializable]];
    enum class TextureFilter
    {
        NEAREST [[tbx::name("nearest")]],
        LINEAR [[tbx::name("linear")]]
    };

    [[tbx::serializable]];
    enum class TextureWrap
    {
        CLAMP_TO_EDGE [[tbx::name("clamp_to_edge")]],
        MIRRORED_REPEAT [[tbx::name("mirrored_repeat")]],
        REPEAT [[tbx::name("repeat")]]
    };

    [[tbx::serializable]];
    enum class TextureFormat
    {
        RGB [[tbx::name("rgb")]],
        RGBA [[tbx::name("rgba")]]
    };

    [[tbx::serializable]];
    enum class TextureMipmaps
    {
        DISABLED [[tbx::name("disabled")]],
        ENABLED [[tbx::name("enabled")]]
    };

    [[tbx::serializable]];
    enum class TextureCompression
    {
        DISABLED [[tbx::name("disabled")]],
        AUTO [[tbx::name("auto")]]
    };

    /// @brief
    /// Purpose: Stores texture sampling, surface settings, and pixel data.
    /// @details
    /// Ownership: Owns texture pixel data by value.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.
    [[tbx::serializable]];
    [[tbx::version(1U)]];
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
        [[tbx::meta]]
        TextureWrap wrap = TextureWrap::REPEAT;

        [[tbx::meta]]
        TextureFilter filter = TextureFilter::LINEAR;

        [[tbx::meta]]
        TextureFormat format = TextureFormat::RGB;

        [[tbx::meta]]
        TextureMipmaps mipmaps = TextureMipmaps::ENABLED;

        [[tbx::meta]]
        TextureCompression compression = TextureCompression::DISABLED;
        std::vector<Pixel> pixels = {255, 255, 255};
    };

    struct TBX_API RenderTexture : Texture
    {
    };

}

#include "tbx/types/assets/texture.generated.h"
