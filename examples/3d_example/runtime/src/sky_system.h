#pragma once
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/time/delta_time.h"

namespace three_d_example
{
    class SkySystem final
    {
      public:
        SkySystem() = default;
        explicit SkySystem(tbx::Entity sky_entity);

      public:
        SkySystem(const SkySystem&) = delete;
        SkySystem(SkySystem&&) = delete;
        SkySystem& operator=(const SkySystem&) = delete;
        SkySystem& operator=(SkySystem&&) = delete;

      public:
        void set_sky_entity(tbx::Entity sky_entity);
        void update(const tbx::DeltaTime& dt) const;

      private:
        tbx::Entity _sky_entity = {};
    };
}
