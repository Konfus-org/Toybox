#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Ids/UsesUID.h"
#include "Tbx/Math/Size.h"
#include "Tbx/Debug/Debugging.h"
#include <string>
#include <array>

namespace Tbx
{
    using Pixel = unsigned char;

    enum class EXPORT TextureFilter
    {
        Nearest,
        Linear
    };

    enum class EXPORT TextureWrap
    {
        ClampToEdge,
        MirroredRepeat,
        Repeat
    };

    enum class EXPORT TextureFormat
    {
        RGB,
        RGBA
    };

    /// <summary>
    /// A texture in RGBA format
    /// </summary>
    struct Texture : public UsesUid
    {
    public:
        /// <summary>
        /// Defaults to a 1x1 white texture
        /// </summary>
        EXPORT Texture() = default;

        /// <summary>
        /// Pass data to be owned by the texture struct.
        /// </summary>
        EXPORT Texture(const Size& size, TextureWrap wrap, TextureFilter filter, TextureFormat format, std::vector<Pixel> pixels)
            : _width(size.Width), _height(size.Height), _wrap(wrap), _filter(filter), _format(format), _pixels(pixels) {}

        EXPORT const std::vector<Pixel>& GetPixels() const { return _pixels; }

        EXPORT uint GetWidth() const { return _width; }
        EXPORT uint GetHeight() const { return _height; }

        EXPORT TextureWrap GetWrap() const { return _wrap; }
        EXPORT TextureFilter GetFilter() const { return _filter; }
        EXPORT TextureFormat GetFormat() const { return _format; }

        EXPORT int GetChannels() const
        {
            switch (_format)
            {
                case Tbx::TextureFormat::RGB:
                    return 3;
                case Tbx::TextureFormat::RGBA:
                    return 4;
                default:
                    TBX_ASSERT(false, "Texture format not supported!");
                    return 0;
            }
        }

    private:
        uint _width = 1;
        uint _height = 1;

        TextureWrap _wrap = TextureWrap::Repeat;
        TextureFilter _filter = TextureFilter::Nearest;
        TextureFormat _format = TextureFormat::RGB;

        std::vector<Pixel> _pixels = { 255, 255, 255 };
    };
}
