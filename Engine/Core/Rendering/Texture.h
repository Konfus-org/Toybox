#pragma once

namespace Toybox
{
    struct TBX_API Texture
    {
    public:
        Texture() = default;
        virtual ~Texture() = default;

        float GetWidth() const { return _width; }
        float GetHeight() const { return _height; }

    private:
        float _width = 0.0f;
        float _height = 0.0f;
    };
}
