#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Ids/UsesUID.h"
#include "Tbx/Math/Size.h"
#include <string>
#include <memory>

namespace Tbx
{
    using TextureData = unsigned char;

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

    struct Texture : public UsesUID
    {
    public:
        /// <summary>
        /// Defaults to a 1x1 white texture
        /// </summary>
        EXPORT Texture() = default;

        /// <summary>
        /// Pass data to be owned by the texture struct.
        /// </summary>
        EXPORT Texture(const Size& size, int channels, std::shared_ptr<TextureData> data);

        EXPORT Texture(const std::string_view& path)
            : _path(path) {
        }

        EXPORT std::shared_ptr<TextureData> GetData() const { return _data; }
        EXPORT std::string GetPath() const { return _path; }

        EXPORT uint GetWidth() const { return _width; }
        EXPORT uint GetHeight() const { return _height; }
        EXPORT int GetChannels() const { return _channels; }
        EXPORT TextureWrap GetWrap() const { return _wrap; }
        EXPORT TextureFilter GetFilter() const { return _filter; }

    private:
        uint _width = 1;
        uint _height = 1;
        int _channels = 0;
        TextureWrap _wrap = TextureWrap::Repeat;
        TextureFilter _filter = TextureFilter::Nearest;

        std::shared_ptr<TextureData> _data = std::make_shared<TextureData>(255);
        std::string _path = "";
    };
}
