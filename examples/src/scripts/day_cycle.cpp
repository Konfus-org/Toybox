#include "day_cycle.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/types/color.h"
#include "tbx/types/components/light.h"
#include "tbx/types/components/sky.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/quaternions.h"
#include "tbx/types/vectors.h"
#include <algorithm>
#include <cmath>

namespace tbx_example
{
    static constexpr float TBX_TAU = 6.28318530717958647692F;

    static float clamp01(const float value)
    {
        return std::clamp(value, 0.0F, 1.0F);
    }

    static float smoothstep01(const float edge0, const float edge1, const float x)
    {
        const float t = clamp01((x - edge0) / (edge1 - edge0));
        return t * t * (3.0F - 2.0F * t);
    }

    static tbx::Color lerp_color(const tbx::Color& a, const tbx::Color& b, const float t)
    {
        return tbx::Color(
            a.r + (b.r - a.r) * t,
            a.g + (b.g - a.g) * t,
            a.b + (b.b - a.b) * t,
            1.0F);
    }

    void DayCycle::on_start()
    {
        _world = get_world_ptr();
        if (auto world = _world.lock())
            _sky = world->get(sky_entity);
        if (!_sky.get_id().is_valid())
            TBX_TRACE_WARNING("DayCycle could not find the authored sky entity.");
    }

    void DayCycle::on_update(const tbx::DeltaTime& dt)
    {
        const auto seconds = static_cast<float>(dt.seconds);
        _elapsed_seconds += seconds;

        const float length = std::max(day_length_seconds, 0.001F);
        const float phase = std::fmod(_elapsed_seconds / length, 1.0F); // [0,1)
        const float sun_height = std::sin(phase * TBX_TAU); // -1 (midnight) .. 1 (noon)
        const float day = smoothstep01(-0.15F, 0.30F, sun_height); // 0 night .. 1 day
        const float night = 1.0F - day;

        // A single directional light plays both roles: warm sun by day, dim blue moon by night.
        const tbx::Color sun_color(1.0F, 0.95F, 0.85F, 1.0F);
        const tbx::Color moon_color(0.45F, 0.6F, 1.0F, 1.0F);

        auto& sun = get_entity();
        if (sun.has_component<tbx::DirectionalLight>() && sun.has_component<tbx::Transform>())
        {
            auto& light = sun.get_component<tbx::DirectionalLight>();
            light.color = lerp_color(moon_color, sun_color, day);
            light.intensity = moon_intensity + (sun_intensity - moon_intensity) * day;
            light.ambient = 0.04F + 0.16F * day;

            // The sun/moon circles the sky (yaw) at a constant downward pitch, so shadows sweep but
            // the light never shines upward from below the horizon.
            auto& transform = sun.get_component<tbx::Transform>();
            transform.rotation = tbx::Quat(tbx::Vec3(-0.6F, phase * TBX_TAU, 0.0F));
        }

        if (_sky.get_id().is_valid() && _sky.has_component<tbx::Sky>()
            && _sky.has_component<tbx::Transform>())
        {
            _sky_yaw = std::fmod(_sky_yaw + sky_spin_speed * seconds, TBX_TAU);
            auto& sky_transform = _sky.get_component<tbx::Transform>();
            sky_transform.rotation = tbx::Quat(tbx::Vec3(0.0F, _sky_yaw, 0.0F));

            // Blend the bright (day) sky toward the dark (night) sky and dim it as night falls. The
            // override re-packs the sky material each frame (the cache re-uploads on every add).
            auto& sky = _sky.get_component<tbx::Sky>();
            sky.material.set_float("blend_factor", night);
            sky.material.set_float("brightness", 0.3F + 0.7F * day);
            sky.material.overrides.has_parameter_override = true;
        }
    }
}
