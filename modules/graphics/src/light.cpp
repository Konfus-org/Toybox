#include "tbx/graphics/light.h"

namespace tbx
{
    Light::Light() = default;

    PointLight::PointLight() = default;

    SpotLight::SpotLight() = default;

    AreaLight::AreaLight() = default;

    DirectionalLight::DirectionalLight() = default;

    DirectionalLight::DirectionalLight(const RgbaColor& col, float inten)
        : Light()
    {
        color = col;
        intensity = inten;
    }
}
