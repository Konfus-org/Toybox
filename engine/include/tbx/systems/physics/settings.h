#pragma once
#include "tbx/systems/physics/settings.generated.h"
#include "tbx/tbx_api.h"
#include "tbx/types/vectors.h"

namespace tbx
{
    /// @brief
    /// Purpose: Defines global physics simulation settings shared across the running application.
    /// @details
    /// Ownership: Owns all configuration values by value.
    /// Thread Safety: Not thread-safe; synchronize access externally.
    [[serializable]];
    struct TBX_API PhysicsSettings
    {
        [[prop]]
        Vec3 gravity = Vec3(0.0F, -9.81F, 0.0F);

        [[prop]]
        float fixed_time_step_seconds = 1.0F / 60.0F;

        [[prop]]
        std::uint32_t max_sub_steps = 4U;

        [[prop]]
        std::uint32_t max_body_count = 65536U;

        [[prop]]
        std::uint32_t max_contact_constraints = 65536U;

        [[prop]]
        std::uint32_t max_body_pairs = 65536U;

        [[prop]]
        std::uint32_t solver_velocity_iterations = 8U;

        [[prop]]
        std::uint32_t solver_position_iterations = 2U;

        [[prop]]
        float max_linear_velocity = 500.0F;

        [[prop]]
        float max_angular_velocity = 200.0F;
    };
}
