#pragma once
#include "tbx/debugging/printable.h"
#include "tbx/tbx_api.h"
#include <string>

namespace tbx
{
    struct TBX_API Bounds
    {
    public:
        Bounds() = default;
        Bounds(float left, float right, float top, float bottom) : Left(left), Right(right), Top(top), Bottom(bottom) {}

        static Bounds FromOrthographicProjection(float size, float aspect);
        static Bounds FromPerspectiveProjection(float fov, float aspectRatio, float zNear);

        static Bounds Identity;

        float Left = 0.0f;
        float Right = 0.0f;
        float Top = 0.0f;
        float Bottom = 0.0f;
    };

    std::string to_string(const Bounds& bounds);
}
