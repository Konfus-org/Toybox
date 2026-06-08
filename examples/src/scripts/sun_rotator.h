#pragma once
#include "sun_rotator.generated.h"
#include "tbx/systems/scripting/script.h"

namespace tbx_example
{
    /// @brief
    /// Purpose: Rotates the sun entity in sync with the authored sky rotation.
    // TODO: We only have gameplay scripts, make just ONE script type.
    // TODO: Make ONE RotateOverTime script and make the sun and sky use it.
    [[tbx::script]];
    [[tbx::version(1U)]];
    class SunRotator final : public tbx::GameplayScript
    {
      public:
        SunRotator() = default;
        ~SunRotator() noexcept override = default;

      public:
        SunRotator(const SunRotator&) = delete;
        SunRotator& operator=(const SunRotator&) = delete;
        SunRotator(SunRotator&&) noexcept = delete;
        SunRotator& operator=(SunRotator&&) noexcept = delete;

      public:
        void on_update(const tbx::DeltaTime& dt) override;

      public:
        [[tbx::prop]]
        float rotation_speed_radians_per_second = 0.01F;
    };
}
