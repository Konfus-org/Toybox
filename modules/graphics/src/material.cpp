#include "tbx/graphics/material.h"
#include <variant>

namespace tbx
{
    static Color try_get_color_or_default(
        const MaterialParameterBindings& parameters,
        const std::string_view name,
        const Color& fallback)
    {
        const auto* parameter = parameters.get(name);
        if (!parameter)
            return fallback;
        if (const auto* color = std::get_if<Color>(&parameter->value))
            return *color;
        return fallback;
    }

    static float try_get_float_or_default(
        const MaterialParameterBindings& parameters,
        const std::string_view name,
        const float fallback)
    {
        const auto* parameter = parameters.get(name);
        if (!parameter)
            return fallback;
        if (const auto* value = std::get_if<float>(&parameter->value))
            return *value;
        if (const auto* value = std::get_if<double>(&parameter->value))
            return static_cast<float>(*value);
        if (const auto* value = std::get_if<int>(&parameter->value))
            return static_cast<float>(*value);
        return fallback;
    }

    StandardMaterialInstance::StandardMaterialInstance()
    {
        set_diffuse(Handle());
        set_normal(Handle());
        set_color(Color(1.0f, 1.0f, 1.0f, 1.0f));
        set_metallic(0.0f);
        set_roughness(1.0f);
        set_emissive(Color(0.0f, 0.0f, 0.0f, 1.0f));
        set_occlusion(1.0f);
        set_alpha_cutoff(0.1f);
        set_exposure(1.0f);
    }

    StandardMaterialInstance::StandardMaterialInstance(
        const Color color,
        const Color emissive,
        const float metallic,
        const float roughness,
        const float occlusion,
        const float alpha_cutoff,
        const Handle& diffuse,
        const Handle& normal,
        const Handle& material_handle)
        : StandardMaterialInstance()
    {
        set_color(color);
        set_emissive(emissive);
        set_metallic(metallic);
        set_roughness(roughness);
        set_occlusion(occlusion);
        set_alpha_cutoff(alpha_cutoff);
        set_diffuse(diffuse);
        set_normal(normal);
        handle = material_handle;
    }

    StandardMaterialInstance::StandardMaterialInstance(const MaterialInstance& other)
        : StandardMaterialInstance()
    {
        set_color(
            try_get_color_or_default(other.parameters, "color", Color(1.0f, 1.0f, 1.0f, 1.0f)));
        set_emissive(
            try_get_color_or_default(other.parameters, "emissive", Color(0.0f, 0.0f, 0.0f, 1.0f)));
        set_metallic(try_get_float_or_default(other.parameters, "metallic", 0.0f));
        set_roughness(try_get_float_or_default(other.parameters, "roughness", 1.0f));
        set_occlusion(try_get_float_or_default(other.parameters, "occlusion", 1.0f));
        set_alpha_cutoff(try_get_float_or_default(other.parameters, "alpha_cutoff", 0.1f));
        set_exposure(try_get_float_or_default(other.parameters, "exposure", 1.0f));
        const auto* diffuse = other.textures.get("diffuse");
        const auto* normal = other.textures.get("normal");
        set_diffuse(diffuse ? diffuse->texture.handle : Handle());
        set_normal(normal ? normal->texture.handle : Handle());
        handle = other.handle;
        has_loaded_defaults = other.has_loaded_defaults;
    }

    void StandardMaterialInstance::set_diffuse(Handle value)
    {
        textures.set("diffuse", std::move(value));
    }

    Handle StandardMaterialInstance::get_diffuse() const
    {
        const auto* texture = textures.get("diffuse");
        if (!texture)
            return {};
        return texture->texture.handle;
    }

    void StandardMaterialInstance::set_normal(Handle value)
    {
        textures.set("normal", std::move(value));
    }

    Handle StandardMaterialInstance::get_normal() const
    {
        const auto* texture = textures.get("normal");
        if (!texture)
            return {};
        return texture->texture.handle;
    }

    void StandardMaterialInstance::set_color(Color value)
    {
        parameters.set("color", std::move(value));
    }

    Color StandardMaterialInstance::get_color() const
    {
        return try_get_color_or_default(parameters, "color", Color(1.0f, 1.0f, 1.0f, 1.0f));
    }

    void StandardMaterialInstance::set_metallic(float value)
    {
        parameters.set("metallic", value);
    }

    float StandardMaterialInstance::get_metallic() const
    {
        return try_get_float_or_default(parameters, "metallic", 0.0f);
    }

    void StandardMaterialInstance::set_roughness(float value)
    {
        parameters.set("roughness", value);
    }

    float StandardMaterialInstance::get_roughness() const
    {
        return try_get_float_or_default(parameters, "roughness", 1.0f);
    }

    void StandardMaterialInstance::set_emissive(Color value)
    {
        parameters.set("emissive", std::move(value));
    }

    Color StandardMaterialInstance::get_emissive() const
    {
        return try_get_color_or_default(parameters, "emissive", Color(0.0f, 0.0f, 0.0f, 1.0f));
    }

    void StandardMaterialInstance::set_occlusion(float value)
    {
        parameters.set("occlusion", value);
    }

    float StandardMaterialInstance::get_occlusion() const
    {
        return try_get_float_or_default(parameters, "occlusion", 1.0f);
    }

    void StandardMaterialInstance::set_alpha_cutoff(float value)
    {
        parameters.set("alpha_cutoff", value);
    }

    float StandardMaterialInstance::get_alpha_cutoff() const
    {
        return try_get_float_or_default(parameters, "alpha_cutoff", 0.1f);
    }

    void StandardMaterialInstance::set_exposure(float value)
    {
        parameters.set("exposure", value);
    }

    float StandardMaterialInstance::get_exposure() const
    {
        return try_get_float_or_default(parameters, "exposure", 1.0f);
    }

}
