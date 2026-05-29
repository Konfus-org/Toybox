#include "sky_rotator.h"
#include "tbx/types/components/sky.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/quaternions.h"

namespace three_d_example
{
    void SkyRotator::on_update(const tbx::DeltaTime& dt)
    {
        auto& sky_entity = get_entity();
        if (!sky_entity.has_component<tbx::Sky>() || !sky_entity.has_component<tbx::Transform>())
            return;

        auto& transform = sky_entity.get_component<tbx::Transform>();
        const auto step = tbx::Quat(
            tbx::Vec3(
                0.0F,
                rotation_speed_radians_per_second * static_cast<float>(dt.seconds),
                0.0F));
        transform.rotation = tbx::normalize(transform.rotation * step);
    }
}
