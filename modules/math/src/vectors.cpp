#include "tbx/math/vectors.h"

namespace tbx
{
    Vec2 normalize(Vec2 v)
    {
        return glm::normalize(v);
    }

    Vec3 normalize(Vec3 v)
    {
        return glm::normalize(v);
    }

    Vec4 normalize(Vec4 v)
    {
        return glm::normalize(v);
    }

    float distance(const Vec3& a, const Vec3& b)
    {
        return glm::distance(a, b);
    }
}
