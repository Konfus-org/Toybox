#include "Tbx/PCH.h"
#include "Tbx/Math/Bounds.h"
#include "Tbx/Math/Trig.h"

namespace Tbx
{
    TBX_EXPORT Bounds Bounds::Identity = Bounds(-1.0f, 1.0f, -1.0f, 1.0f);

    std::string Bounds::ToString() const { return std::format("[Left: {}, Right: {}, Top: {}, Bottom: {}]", Left, Right, Top, Bottom); }

    Bounds Bounds::FromOrthographicProjection(float size, float aspect)
    {
        float halfWidth = (size * aspect);
        float halfHeight = size;

        float left = -halfWidth;
        float right = halfWidth;
        float top = halfHeight;
        float bottom = -halfHeight;

        return { left, right, top, bottom };
    }

    Bounds Bounds::FromPerspectiveProjection(float fov, float aspectRatio, float zNear)
    {
        float scale = Tan(fov * 0.5f * zNear);

        const float left = aspectRatio * -scale;
        const float right = -left;
        const float top = scale;
        const float bottom = -top;

        return { left, right, top, bottom };
    }
}
