#pragma once

namespace Toybox::Math
{
    struct Size
    {
        int Width, Height;

        Size(int width, int height)
        {
            Width = width;
            Height = height;
        }
    };
}