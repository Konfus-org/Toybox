#include "tbx/types/quaternions.h"
#include <cmath>
#include <glm/gtc/quaternion.hpp>

namespace tbx
{
    Quat normalize(Quat q)
    {
        return glm::normalize(q);
    }

    Quat look_rotation(const Vec3& forward, const Vec3& world_up)
    {
        const auto f = glm::normalize(forward);
        // Near-vertical looks have no stable horizon against world up; fall back to a different axis.
        auto up_reference = world_up;
        if (std::abs(glm::dot(f, up_reference)) > 0.999F)
            up_reference = Vec3(0.0F, 0.0F, 1.0F);

        const auto right = glm::normalize(glm::cross(f, up_reference));
        const auto up = glm::cross(right, f);
        return glm::normalize(glm::quat_cast(glm::mat3(right, up, -f)));
    }
}
