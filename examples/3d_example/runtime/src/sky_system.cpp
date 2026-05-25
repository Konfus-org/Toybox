#include "sky_system.h"
#include "tbx/types/components/sky.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/quaternions.h"
#include <utility>

namespace three_d_example
{
    SkySystem::SkySystem(tbx::Entity sky_entity)
        : _sky_entity(std::move(sky_entity))
    {
    }

    void SkySystem::set_sky_entity(tbx::Entity sky_entity)
    {
        _sky_entity = std::move(sky_entity);
    }

    void SkySystem::update(const tbx::DeltaTime& dt) const
    {
        if (!_sky_entity.get_id().is_valid() || !_sky_entity.has_component<tbx::Sky>()
            || !_sky_entity.has_component<tbx::Transform>())
        {
            return;
        }

        constexpr float rotation_speed_radians_per_second = 0.01F;
        auto& transform = _sky_entity.get_component<tbx::Transform>();
        const auto step = tbx::Quat(
            tbx::Vec3(
                0.0F,
                rotation_speed_radians_per_second * static_cast<float>(dt.seconds),
                0.0F));
        transform.rotation = tbx::normalize(transform.rotation * step);
    }
}
