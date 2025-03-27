#pragma once
#include "Core/ToolboxAPI.h"
#include "Core/Math/MathAPI.h"
#include "Core/Ids/UID.h"
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

        TBX_API UID GetId() const { return _id; }
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
        UID _id;

        std::shared_ptr<TextureData> _data = nullptr;
        std::string _path = "";
    };

    struct TextureRenderData
    {
    public:
        TBX_API TextureRenderData(const Texture& texture, const uint& slot = 0) : _texture(texture), _slot(slot) {}
        TBX_API ~TextureRenderData() = default;

        TBX_API uint GetSlot() const { return _slot; }
        TBX_API const Texture& GetTexture() const { return _texture; }
    private:
        uint _slot;
        Texture _texture;
    };
}
