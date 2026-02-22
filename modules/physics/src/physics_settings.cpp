#include "tbx/physics/physics_settings.h"
#include <cmath>

namespace tbx
{
    PhysicsSettings::PhysicsSettings(IMessageDispatcher& dispatcher)
        : gravity(&dispatcher, this, &PhysicsSettings::gravity, Vec3(0.0F, -9.81F, 0.0F))
        , fixed_time_step_seconds(
              &dispatcher,
              this,
              &PhysicsSettings::fixed_time_step_seconds,
              1.0F / 60.0F)
        , max_sub_steps(&dispatcher, this, &PhysicsSettings::max_sub_steps, 4)
        , max_body_count(&dispatcher, this, &PhysicsSettings::max_body_count, 65536)
        , max_contact_constraints(
              &dispatcher,
              this,
              &PhysicsSettings::max_contact_constraints,
              65536)
        , max_body_pairs(&dispatcher, this, &PhysicsSettings::max_body_pairs, 65536)
        , solver_velocity_iterations(
              &dispatcher,
              this,
              &PhysicsSettings::solver_velocity_iterations,
              8)
        , solver_position_iterations(
              &dispatcher,
              this,
              &PhysicsSettings::solver_position_iterations,
              2)
        , max_linear_velocity(&dispatcher, this, &PhysicsSettings::max_linear_velocity, 500.0F)
        , max_angular_velocity(&dispatcher, this, &PhysicsSettings::max_angular_velocity, 200.0F)
    {
    }

    bool PhysicsSettings::get_is_valid() const
    {
        if (!std::isfinite(gravity.value.x) || !std::isfinite(gravity.value.y)
            || !std::isfinite(gravity.value.z))
            return false;

        if (!std::isfinite(fixed_time_step_seconds.value) || fixed_time_step_seconds.value <= 0.0F)
            return false;

        if (max_sub_steps.value == 0)
            return false;

        if (max_body_count.value == 0 || max_contact_constraints.value == 0
            || max_body_pairs.value == 0)
            return false;

        if (solver_velocity_iterations.value == 0 || solver_position_iterations.value == 0)
            return false;

        if (!std::isfinite(max_linear_velocity.value) || max_linear_velocity.value <= 0.0F)
            return false;

        if (!std::isfinite(max_angular_velocity.value) || max_angular_velocity.value <= 0.0F)
            return false;

        return true;
    }
}
