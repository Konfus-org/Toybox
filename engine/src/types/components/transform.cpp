#include "tbx/types/components/transform.h"
#include "types/components/internal/transform_internal.h"
#include <cmath>
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

    Transform world_to_local_tranform(const Transform& parent_world, const Transform& world)
    {
        const Quat parent_world_rotation = normalize(parent_world.rotation);
        const Quat inverse_parent_rotation = glm::inverse(parent_world_rotation);
        auto local = Transform {};
        local.scale = internal::divide_components(world.scale, parent_world.scale);
        local.rotation = normalize(inverse_parent_rotation * world.rotation);
        local.position = inverse_parent_rotation
                         * internal::divide_components(
                             world.position - parent_world.position,
                             parent_world.scale);
        return local;
    }
}
