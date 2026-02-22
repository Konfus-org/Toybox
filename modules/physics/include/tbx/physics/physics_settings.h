#pragma once
#include "tbx/math/vectors.h"
#include "tbx/messages/observable.h"
#include "tbx/tbx_api.h"
#include <cstdint>

namespace tbx
{
    /// <summary>
    /// Purpose: Defines global physics simulation settings shared across the running application.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns all configuration values by value.
    /// Thread Safety: Not thread-safe; synchronize access externally.
    /// </remarks>
    struct TBX_API PhysicsSettings
    {
        PhysicsSettings(IMessageDispatcher& dispatcher);

        Observable<PhysicsSettings, Vec3> gravity;
        Observable<PhysicsSettings, float> fixed_time_step_seconds;
        Observable<PhysicsSettings, std::uint32_t> max_sub_steps;

        Observable<PhysicsSettings, std::uint32_t> max_body_count;
        Observable<PhysicsSettings, std::uint32_t> max_contact_constraints;
        Observable<PhysicsSettings, std::uint32_t> max_body_pairs;

        Observable<PhysicsSettings, std::uint32_t> solver_velocity_iterations;
        Observable<PhysicsSettings, std::uint32_t> solver_position_iterations;
        Observable<PhysicsSettings, float> max_linear_velocity;
        Observable<PhysicsSettings, float> max_angular_velocity;
    };
}
