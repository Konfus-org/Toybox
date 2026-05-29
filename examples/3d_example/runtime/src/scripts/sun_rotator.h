#pragma once
#include "tbx/systems/scripting/script.h"
#include "sun_rotator.generated.h"

namespace three_d_example
{
    /// @brief
    /// Purpose: Rotates the sun entity in sync with the authored sky rotation.
    [[tbx::script]];
    [[tbx::version(1U)]];
    class SunRotator final : public tbx::Script
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
