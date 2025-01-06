#pragma once
#include "TbxAPI.h"
#include "Operations.h"

namespace Tbx
{
    struct TBX_API Bounds
    {
    public:
        float Left;
        float Right;
        float Top;
        float Bottom;

        static Bounds FromOrthographicProjection(float size, float aspect)
        {
            auto orthographicSize = size / 2.0f;
            float halfWidth = (orthographicSize * aspect) + (aspect * 0.1f);
            float halfHeight = orthographicSize + (aspect * 0.1f);

            float left = halfWidth;
            float right = -halfWidth;
            float top = halfHeight;
            float bottom = -halfHeight;

            return { left, right, top, bottom };
        }

        static Bounds FromPerspectiveProjection(float fov, float aspectRatio, float zNear)
        {
            float scale = Math::Tan(fov * 0.5f * Math::PI / 180) * zNear;
            const float right = aspectRatio * scale;
            const float left = -right;
            const float top = scale;
            const float bottom = -top;

            return { left, right, top, bottom };
        }

        static Bounds Identity()
        {
            return { -1.0f, 1.0f, -1.0f, 1.0f };
        }
    };
}
