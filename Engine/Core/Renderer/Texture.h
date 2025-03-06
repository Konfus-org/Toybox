#pragma once
#include "TbxAPI.h"
#include "TbxPCH.h"
#include "Math/MathAPI.h"
#include "Util/ID.h"
#include <string>

namespace Tbx
{
    using TextureData = unsigned char;

    enum class TBX_API TextureFilter
    {
        Nearest,
        Linear
    };

    enum class TBX_API TextureWrap
    {
        ClampToEdge,
        MirroredRepeat,
        Repeat
    };

    struct Texture
    {
    public:
        TBX_API Texture() = default;
        TBX_API explicit(false) Texture(const std::string& path);
        TBX_API ~Texture() = default;

        TBX_API std::shared_ptr <TextureData> GetData() const { return _data; }

        TBX_API std::string GetPath() const { return _path; }

        TBX_API ID GetId() const { return _id; }
        TBX_API uint GetWidth() const { return _width; }
        TBX_API uint GetHeight() const { return _height; }
        TBX_API int GetChannels() const { return _channels; }
        TBX_API TextureWrap GetWrap() const { return _wrap; }
        TBX_API TextureFilter GetFilter() const { return _filter; }

    private:
        uint _width = 0;
        uint _height = 0;
        int _channels = 0;
        TextureWrap _wrap = TextureWrap::Repeat;
        TextureFilter _filter = TextureFilter::Nearest;
        ID _id;

        std::shared_ptr<TextureData> _data = nullptr;
        std::string _path = "";
    };
}
