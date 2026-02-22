#include "tbx/physics/physics.h"
#include <cmath>

namespace tbx
{
    bool Physics::get_is_valid() const
    {
        if (!std::isfinite(mass) || mass <= 0.0F)
            return false;

        if (!std::isfinite(linear_velocity.x) || !std::isfinite(linear_velocity.y)
            || !std::isfinite(linear_velocity.z))
            return false;

        if (!std::isfinite(angular_velocity.x) || !std::isfinite(angular_velocity.y)
            || !std::isfinite(angular_velocity.z))
            return false;

        if (!std::isfinite(friction) || friction < 0.0F)
            return false;

        if (!std::isfinite(restitution) || restitution < 0.0F)
            return false;

        if (!std::isfinite(default_linear_damping) || default_linear_damping < 0.0F)
            return false;

        if (!std::isfinite(default_angular_damping) || default_angular_damping < 0.0F)
            return false;

        if (!std::isfinite(sleep_velocity_threshold) || sleep_velocity_threshold < 0.0F)
            return false;

        if (!std::isfinite(sleep_time_seconds) || sleep_time_seconds < 0.0F)
            return false;

        return true;
    }
}
