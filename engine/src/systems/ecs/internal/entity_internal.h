#pragma once
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/types/uuid.h"
#include <cstddef>

namespace tbx::internal
{
    static Vec3 multiply_components(const Vec3& left, const Vec3& right)
    {
        return Vec3(left.x * right.x, left.y * right.y, left.z * right.z);
    }

    static Transform compose_world_space_transform(
        const Transform& parent_transform,
        const Transform& local_transform)
    {
        auto world_transform = Transform {};
        world_transform.scale = multiply_components(parent_transform.scale, local_transform.scale);
        world_transform.rotation = normalize(parent_transform.rotation * local_transform.rotation);
        world_transform.position =
            parent_transform.position
            + (parent_transform.rotation
               * multiply_components(parent_transform.scale, local_transform.position));
        return world_transform;
    }

}
