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
        /// <summary>
        /// Purpose: Constructs global physics settings with engine defaults.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not assume ownership of the dispatcher reference.
        /// Thread Safety: Not thread-safe; initialize on one thread.
        /// </remarks>
        explicit PhysicsSettings(IMessageDispatcher& dispatcher);

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

        /// <summary>
        /// Purpose: Validates that global simulation settings are usable by runtime backends.
        /// </summary>
        /// <remarks>
        /// Ownership: Reads internal values only and does not transfer ownership.
        /// Thread Safety: Safe for concurrent reads when no concurrent mutation occurs.
        /// </remarks>
        bool get_is_valid() const;
    };
}
