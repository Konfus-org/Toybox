#pragma once
#include "Tbx/Utils/DllExport.h"
#include <string>

namespace Tbx
{
    struct EXPORT Bounds
    {
    public:
        Bounds(float left, float right, float top, float bottom) 
            : Left(left), Right(right), Top(top), Bottom(bottom) {}

        std::string ToString() const;

        static Bounds FromOrthographicProjection(float size, float aspect);
        static Bounds FromPerspectiveProjection(float fov, float aspectRatio, float zNear);

        float Left;
        float Right;
        float Top;
        float Bottom;
    };
}
