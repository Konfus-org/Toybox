#include "tbx/math/bounds.h"
#include "tbx/math/trig.h"
#include <format>

namespace tbx
{
    Bounds::Bounds() = default;

    Bounds::Bounds(float l, float r, float t, float b)
        : left(l)
        , right(r)
        , top(t)
        , bottom(b)
    {
    }

    Bounds Bounds::from_orthographic_projection(float size, float aspect)
    {
        const float half_width = size * aspect;
        const float half_height = size;

        const float left = -half_width;
        const float right = half_width;
        const float top = half_height;
        const float bottom = -half_height;

        return {left, right, top, bottom};
    }

    Bounds Bounds::from_perspective_projection(float fov, float aspect_ratio, float z_near)
    {
        const float scale = tan(fov * 0.5f) * z_near;

        const float left = aspect_ratio * -scale;
        const float right = -left;
        const float top = scale;
        const float bottom = -top;

        return {left, right, top, bottom};
    }

    std::string to_string(const Bounds& bounds)
    {
        return std::format(
            "[Left: {}, Right: {}, Top: {}, Bottom: {}]",
            bounds.left,
            bounds.right,
            bounds.top,
            bounds.bottom);
    }
}
