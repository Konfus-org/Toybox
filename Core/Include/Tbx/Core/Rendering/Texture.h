#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/Math/MathAPI.h"
#include "Tbx/Core/Ids/UsesUID.h"
#include <string>

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

    // TODO: Seperate loading of texture and texture data
    struct Texture : public UsesUID
    {
    public:
        EXPORT Texture() = default;
        EXPORT explicit(false) Texture(const std::string& path);

        EXPORT std::shared_ptr <TextureData> GetData() const { return _data; }
        EXPORT std::string GetPath() const { return _path; }

        EXPORT uint GetWidth() const { return _width; }
        EXPORT uint GetHeight() const { return _height; }
        EXPORT int GetChannels() const { return _channels; }
        EXPORT TextureWrap GetWrap() const { return _wrap; }
        EXPORT TextureFilter GetFilter() const { return _filter; }

    private:
        uint _width = 0;
        uint _height = 0;
        int _channels = 0;
        TextureWrap _wrap = TextureWrap::Repeat;
        TextureFilter _filter = TextureFilter::Nearest;

        std::shared_ptr<TextureData> _data = nullptr;
        std::string _path = "";
    };
}
