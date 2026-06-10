#include "bob_and_rotate.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/quaternions.h"
#include "tbx/types/vectors.h"
#include <cmath>

namespace tbx_example
{
    void BobAndRotate::on_update(const tbx::DeltaTime& dt)
    {
        auto& entity = get_entity();
        if (!entity.has_component<tbx::Transform>())
            return;

        auto& transform = entity.get_component<tbx::Transform>();
        const auto seconds = static_cast<float>(dt.seconds);

        // Spin about Y at the authored rate.
        const auto step =
            tbx::Quat(tbx::Vec3(0.0F, rotation_speed_radians_per_second * seconds, 0.0F));
        transform.rotation = tbx::normalize(transform.rotation * step);

        // Bob by applying the frame-to-frame delta so the motion never drifts; an amplitude of 0
        // (sky/sun) leaves the position untouched.
        _elapsed_seconds += seconds;
        const auto offset = bob_amplitude * std::sin(_elapsed_seconds * bob_frequency);
        transform.position.y += offset - _previous_bob_offset;
        _previous_bob_offset = offset;
    }
}
