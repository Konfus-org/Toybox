#include "tbx/graphics/light.h"

namespace tbx
{
    Light::Light() = default;

    PointLight::PointLight() = default;

    SpotLight::SpotLight() = default;

    AreaLight::AreaLight() = default;

    DirectionalLight::DirectionalLight() = default;

    DirectionalLight::DirectionalLight(RgbaColor col, float inten)
        : Light()
    {
        color = std::move(col);
        intensity = inten;
    }

    DirectionalLight::DirectionalLight(RgbaColor col, float inten, float amb)
    {
        color = std::move(col);
        intensity = inten;
        ambient = amb;
    }
}
