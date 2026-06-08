#include "sun_rotator.h"
#include "tbx/types/components/light.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/quaternions.h"

namespace tbx_example
{
    void SunRotator::on_update(const tbx::DeltaTime& dt)
    {
        auto& sun_entity = get_entity();
        if (!sun_entity.has_component<tbx::DirectionalLight>()
            || !sun_entity.has_component<tbx::Transform>())
        {
            return;
        }

        auto& transform = sun_entity.get_component<tbx::Transform>();
        const auto step = tbx::Quat(
            tbx::Vec3(
                0.0F,
                rotation_speed_radians_per_second * static_cast<float>(dt.seconds),
                0.0F));
        transform.rotation = tbx::normalize(transform.rotation * step);
    }
}
