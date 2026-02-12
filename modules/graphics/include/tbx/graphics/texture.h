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

    /// <summary>
    /// Purpose: Stores texture sampling and surface settings shared by texture data and texture
    /// instances.
    /// </summary>
    /// <remarks>
    /// Ownership: Value type settings.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.
    /// </remarks>
    struct TBX_API TextureSettings
    {
        Size resolution = {1, 1};
        TextureWrap wrap = TextureWrap::REPEAT;
        TextureFilter filter = TextureFilter::LINEAR;
        TextureFormat format = TextureFormat::RGB;
        TextureMipmaps mipmaps = TextureMipmaps::ENABLED;
        TextureCompression compression = TextureCompression::AUTO;

        bool operator==(const TextureSettings&) const = default;
    };

    struct TBX_API Texture : TextureSettings
    {
        Texture() = default;
        Texture(
            const Size& resolution,
            TextureWrap wrap,
            TextureFilter filter,
            TextureFormat format,
            const std::vector<Pixel>& pixels,
            TextureMipmaps mipmaps = TextureMipmaps::ENABLED,
            TextureCompression compression = TextureCompression::AUTO)
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

    /// <summary>
    /// Purpose: Stores a texture asset handle with optional runtime texture settings overrides.
    /// </summary>
    /// <remarks>
    /// Ownership: Stores a non-owning handle reference and optional value settings.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.
    /// </remarks>
    struct TBX_API TextureInstance
    {
        TextureInstance() = default;
        explicit TextureInstance(Handle texture_handle)
            : handle(std::move(texture_handle))
        {
        }
        TextureInstance(Handle texture_handle, TextureSettings texture_settings)
            : handle(std::move(texture_handle))
            , settings(std::move(texture_settings))
        {
        }

        Handle handle = {};
        std::optional<TextureSettings> settings = std::nullopt;
    };
}
