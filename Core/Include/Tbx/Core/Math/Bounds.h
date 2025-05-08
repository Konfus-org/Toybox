#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/Debug/IPrintable.h"
#include <string>
#include <format>

namespace Tbx
{
    struct EXPORT Bounds : public IPrintable
    {
    public:
        Bounds(float left, float right, float top, float bottom) 
            : Left(left), Right(right), Top(top), Bottom(bottom) {}

        std::string ToString() const override { return std::format("[Left: {}, Right: {}, Top: {}, Bottom: {}]", Left, Right, Top, Bottom); }

        static Bounds FromOrthographicProjection(float size, float aspect);
        static Bounds FromPerspectiveProjection(float fov, float aspectRatio, float zNear);

        float Left;
        float Right;
        float Top;
        float Bottom;
    };
}
