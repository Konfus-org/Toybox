#pragma once

namespace Toybox
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