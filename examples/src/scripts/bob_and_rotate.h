#pragma once
#include "bob_and_rotate.generated.h"
#include "tbx/systems/scripting/script.h"

namespace tbx_example
{
    /// @brief
    /// Purpose: Spins an entity about its Y axis and bobs it up and down over time. Material demo
    /// props use a visible bob; the sky and sun reuse this script with bob_amplitude 0 so they only
    /// rotate (replacing the old single-purpose SkyRotator/SunRotator scripts).
    [[tbx::script]];
    [[tbx::version(1U)]];
    class BobAndRotate final : public tbx::GameplayScript
    {
      public:
        BobAndRotate() = default;
        ~BobAndRotate() noexcept override = default;

      public:
        BobAndRotate(const BobAndRotate&) = delete;
        BobAndRotate& operator=(const BobAndRotate&) = delete;
        BobAndRotate(BobAndRotate&&) noexcept = delete;
        BobAndRotate& operator=(BobAndRotate&&) noexcept = delete;

      public:
        void on_update(const tbx::DeltaTime& dt) override;

      public:
        [[tbx::prop]]
        float rotation_speed_radians_per_second = 1.0F;

        [[tbx::prop]]
        float bob_amplitude = 0.5F;

        [[tbx::prop]]
        float bob_frequency = 1.5F;

      private:
        // Runtime-only bob bookkeeping (not serialized): applying the frame-to-frame delta keeps the
        // motion from accumulating and leaves bob_amplitude-0 entities exactly in place.
        float _elapsed_seconds = 0.0F;
        float _previous_bob_offset = 0.0F;
    };
}
