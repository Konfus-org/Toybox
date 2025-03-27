#pragma once
#include "Core/ToolboxAPI.h"

namespace Tbx
{
    struct TBX_API Bounds
    {
    public:
        float Left;
        float Right;
        float Top;
        float Bottom;

        std::string ToString() const { return std::format("[Left: {}, Right: {}, Top: {}, Bottom: {}]", Left, Right, Top, Bottom); }

        static Bounds Identity()
        {
            return { -1.0f, 1.0f, -1.0f, 1.0f };
        }

        static Bounds FromOrthographicProjection(float size, float aspect);
        static Bounds FromPerspectiveProjection(float fov, float aspectRatio, float zNear);
    };
}
