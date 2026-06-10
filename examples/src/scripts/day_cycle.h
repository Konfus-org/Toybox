#pragma once
#include "day_cycle.generated.h"
#include "tbx/systems/scripting/script.h"
#include "tbx/types/assets/world.h"
#include "tbx/types/uuid.h"
#include <memory>

namespace tbx_example
{
    /// @brief
    /// Purpose: Drives a looping day/night cycle. Attached to the sun (a DirectionalLight) and
    /// given a reference to the sky entity, it slowly rotates the sky, arcs the sun while dropping
    /// its intensity and ambient toward dusk, then swaps the light to a dim blue "moon" and blends
    /// the sky's bright texture toward its dark texture for night — looping forever.
    [[tbx::script]];
    [[tbx::version(1U)]];
    class DayCycle final : public tbx::GameplayScript
    {
      public:
        DayCycle() = default;
        ~DayCycle() noexcept override = default;

      public:
        DayCycle(const DayCycle&) = delete;
        DayCycle& operator=(const DayCycle&) = delete;
        DayCycle(DayCycle&&) noexcept = delete;
        DayCycle& operator=(DayCycle&&) noexcept = delete;

      public:
        void on_start() override;
        void on_update(const tbx::DeltaTime& dt) override;

      public:
        /// @brief The sky entity this cycle rotates and blends (resolved on start).
        [[tbx::prop]]
        tbx::Uuid sky_entity = {};

        /// @brief Seconds for one full day + night loop.
        [[tbx::prop]]
        float day_length_seconds = 30.0F;

        /// @brief Sky spin rate in radians per second.
        [[tbx::prop]]
        float sky_spin_speed = 0.03F;

        /// @brief Peak directional intensity at midday.
        [[tbx::prop]]
        float sun_intensity = 1.2F;

        /// @brief Directional intensity of the night-time moon.
        [[tbx::prop]]
        float moon_intensity = 0.25F;

      private:
        float _elapsed_seconds = 0.0F;
        float _sky_yaw = 0.0F;
        std::weak_ptr<tbx::World> _world = {};
        tbx::Entity _sky = {};
    };
}
