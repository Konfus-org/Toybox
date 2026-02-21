#include "tbx/math/transform.h"

namespace tbx
{
    Transform::Transform() = default;
    Transform::Transform(const Vec3& pos)
        : position(pos)
    {
    }
    Transform::Transform(const Vec3& pos, const Quat& rot)
        : position(pos)
        , rotation(rot)
    {
    }
    Transform::Transform(const Vec3& pos, const Quat& rot, const Vec3& scl)
        : position(pos)
        , rotation(rot)
        , scale(scl)
    {
    }
}
