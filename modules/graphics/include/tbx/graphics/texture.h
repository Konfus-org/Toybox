#pragma once
#include "tbx/common/handle.h"
#include "tbx/math/size.h"
#include "tbx/tbx_api.h"
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
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

    /// TODO: remove texture settings, we DO NOT NEED THIS, we can just use the vanilla Texture
    /// anywhere we need it.
    /// @brief
    /// Purpose: Stores texture sampling and surface settings shared by texture data and texture
    /// instances.
    /// @details
    /// Ownership: Value type settings.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.
    struct TBX_API TextureSettings
    {
        Size resolution = {1, 1};
        TextureWrap wrap = TextureWrap::REPEAT;
        TextureFilter filter = TextureFilter::LINEAR;
        TextureFormat format = TextureFormat::RGB;
        TextureMipmaps mipmaps = TextureMipmaps::ENABLED;
        TextureCompression compression = TextureCompression::DISABLED;

        bool operator==(const TextureSettings& other) const
        {
            return resolution.width == other.resolution.width
                   && resolution.height == other.resolution.height && wrap == other.wrap
                   && filter == other.filter && format == other.format && mipmaps == other.mipmaps
                   && compression == other.compression;
        }
    };

    struct TBX_API Texture : TextureSettings
    {
        Texture() = default;
        Texture(
            const Size& resolution,
            TextureWrap wrap,
            TextureFilter filter,
            TextureFormat format,
            const std::vector<Pixel>& pixels)
            : TextureSettings {.resolution = resolution, .wrap = wrap, .filter = filter, .format = format}
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
            : TextureSettings {
                .resolution = resolution,
                .wrap = wrap,
                .filter = filter,
                .format = format,
                .mipmaps = mipmaps,
                .compression = compression,
            }
            , pixels(pixels)
        {
        }

        std::vector<Pixel> pixels = {255, 255, 255};
    };

    /// TODO: remove texture instance, we shouldn't need this anywhere we can just use the handle.
    /// @brief
    /// Purpose: Stores a texture asset handle with optional runtime texture settings values.
    /// @details
    /// Ownership: Stores a non-owning handle reference and optional value settings.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.
    struct TBX_API TextureInstance
    {
        Handle handle = {};
        std::optional<TextureSettings> settings = std::nullopt;
    };
}
