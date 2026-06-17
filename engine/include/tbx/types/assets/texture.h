#pragma once
#include "tbx/systems/assets/serialization.h"
#include "tbx/types/assets/asset.h"
#include "tbx/types/assets/texture.generated.h"
#include "tbx/types/size.h"
#include "tbx/types/typedefs.h"
#include <utility>
#include <vector>

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
        RGBA [[name("rgba")]],
        RGBA8 [[name("rgba8")]],
        RGBA16_FLOAT [[name("rgba16_float")]],
        RGBA32_FLOAT [[name("rgba32_float")]],
        R8 [[name("r8")]],
        R16_FLOAT [[name("r16_float")]],
        RG8 [[name("rg8")]],
        RG16_FLOAT [[name("rg16_float")]],
        DEPTH24_STENCIL8 [[name("depth24_stencil8")]],
        DEPTH32_FLOAT [[name("depth32_float")]],
    };

    [[serializable]];
    enum class TextureUsage : uint8
    {
        SAMPLED [[name("sampled")]] = 1U << 0U,
        RENDER_TARGET [[name("render_target")]] = 1U << 1U,
        DEPTH_STENCIL [[name("depth_stencil")]] = 1U << 2U,
        STORAGE [[name("storage")]] = 1U << 3U,
        SAMPLED_RENDER_TARGET [[name("sampled_render_target")]] = (1U << 0U) | (1U << 1U),
        SAMPLED_DEPTH_STENCIL [[name("sampled_depth_stencil")]] = (1U << 0U) | (1U << 2U),
    };

    constexpr TextureUsage operator|(const TextureUsage left, const TextureUsage right)
    {
        return static_cast<TextureUsage>(static_cast<uint8>(left) | static_cast<uint8>(right));
    }

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
            std::vector<Pixel> pixels)
            : wrap(wrap)
            , filter(filter)
            , format(format)
            , pixels(std::move(pixels))
            , resolution(resolution)
        {
        }
        Texture(
            const Size& resolution,
            TextureWrap wrap,
            TextureFilter filter,
            TextureFormat format,
            TextureMipmaps mipmaps,
            TextureCompression compression,
            std::vector<Pixel> pixels)
            : wrap(wrap)
            , filter(filter)
            , format(format)
            , mipmaps(mipmaps)
            , compression(compression)
            , pixels(std::move(pixels))
            , resolution(resolution)
        {
        }

        bool operator==(const Texture& other) const
        {
            return resolution.width == other.resolution.width
                   && resolution.height == other.resolution.height && wrap == other.wrap
                   && filter == other.filter && format == other.format && mipmaps == other.mipmaps
                   && compression == other.compression && pixels == other.pixels;
        }

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

        Size resolution = {1, 1};
    };

}
