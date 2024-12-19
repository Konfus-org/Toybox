#pragma once

namespace Toybox
{
    struct Color
    {
    public:
        Color() = default;
        Color(float r, float g, float b, float a) : R(r), G(g), B(b), A(a) {}

        float R;
        float G;
        float B;
        float A;
    };
}