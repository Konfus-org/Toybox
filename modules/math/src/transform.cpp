#include "tbx/math/transform.h"
#include <cmath>

namespace tbx
{
    static Vec3 divide_components(const Vec3& numerator, const Vec3& denominator)
    {
        static constexpr float component_epsilon = 0.000001F;
        auto result = Vec3(0.0F, 0.0F, 0.0F);
        result.x =
            std::abs(denominator.x) <= component_epsilon ? 0.0F : numerator.x / denominator.x;
        result.y =
            std::abs(denominator.y) <= component_epsilon ? 0.0F : numerator.y / denominator.y;
        result.z =
            std::abs(denominator.z) <= component_epsilon ? 0.0F : numerator.z / denominator.z;
        return result;
    }

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
        local.scale = divide_components(world.scale, parent_world.scale);
        local.rotation = normalize(inverse_parent_rotation * world.rotation);
        local.position =
            inverse_parent_rotation
            * divide_components(world.position - parent_world.position, parent_world.scale);
        return local;
    }
}
