#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/Debug/ILoggable.h"
#include <string>
#include <format>

namespace Tbx
{
    struct EXPORT Bounds : public ILoggable
    {
    public:
        Bounds(float left, float right, float top, float bottom) 
            : Left(left), Right(right), Top(top), Bottom(bottom) {}

        float Left;
        float Right;
        float Top;
        float Bottom;

        std::string ToString() const override { return std::format("[Left: {}, Right: {}, Top: {}, Bottom: {}]", Left, Right, Top, Bottom); }

        static Bounds Identity()
        {
            return { -1.0f, 1.0f, -1.0f, 1.0f };
        }

        static Bounds FromOrthographicProjection(float size, float aspect);
        static Bounds FromPerspectiveProjection(float fov, float aspectRatio, float zNear);
    };
}
