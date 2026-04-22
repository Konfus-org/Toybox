#include "tbx/core/systems/graphics/light.h"

namespace tbx
{
    Light::Light() = default;

    PointLight::PointLight() = default;

    PointLight::PointLight(Color col, float inten, float rng)
        : Light()
    {
        color = col;
        intensity = inten;
        range = rng;
    }

    SpotLight::SpotLight() = default;

    SpotLight::SpotLight(Color col, float inten, float rng, float inner, float outer)
        : Light()
    {
        color = col;
        intensity = inten;
        range = rng;
        inner_angle = inner;
        outer_angle = outer;
    }

    AreaLight::AreaLight() = default;

    AreaLight::AreaLight(Color col, float inten, float rng, Vec2 size)
        : Light()
    {
        color = col;
        intensity = inten;
        range = rng;
        area_size = size;
    }

    DirectionalLight::DirectionalLight() = default;

    DirectionalLight::DirectionalLight(Color col, float inten, float amb)
        : Light()
    {
        color = col;
        intensity = inten;
        ambient = amb;
    }
}
