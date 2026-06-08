#pragma once
#include "sky_rotator.generated.h"
#include "tbx/systems/scripting/script.h"

namespace tbx_example
{
    /// @brief
    /// Purpose: Rotates the sky entity authored with this script.
    [[tbx::script]];
    [[tbx::version(1U)]];
    class SkyRotator final : public tbx::GameplayScript
    {
      public:
        SkyRotator() = default;
        ~SkyRotator() noexcept override = default;

      public:
        SkyRotator(const SkyRotator&) = delete;
        SkyRotator& operator=(const SkyRotator&) = delete;
        SkyRotator(SkyRotator&&) noexcept = delete;
        SkyRotator& operator=(SkyRotator&&) noexcept = delete;

      public:
        void on_update(const tbx::DeltaTime& dt) override;

      public:
        [[tbx::prop]]
        float rotation_speed_radians_per_second = 0.01F;
    };
}
