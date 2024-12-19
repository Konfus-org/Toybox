#pragma once

namespace Toybox
{
    struct Vector2
    {
    public:
        Vector2()
        {
            X = 0;
            Y = 0;
        }

        Vector2(float x, float y)
        {
            X = x;
            Y = y;
        }

        float X, Y;
    };
}
