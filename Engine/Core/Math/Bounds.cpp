#include "TbxPCH.h"
#include "Bounds.h"
#include "Trig.h"

namespace Tbx
{
    Bounds Tbx::Bounds::FromOrthographicProjection(float size, float aspect)
    {
        float halfWidth = (size * aspect);
        float halfHeight = size;

        float left = -halfWidth;
        float right = halfWidth;
        float top = halfHeight;
        float bottom = -halfHeight;

        return { left, right, top, bottom };
    }

    Bounds Tbx::Bounds::FromPerspectiveProjection(float fov, float aspectRatio, float zNear)
    {
        float scale = Math::Tan(fov * 0.5f * zNear);

        const float right = aspectRatio * -scale;
        const float left = -right;
        const float top = scale;
        const float bottom = -top;

        return { left, right, top, bottom };
    }
}
