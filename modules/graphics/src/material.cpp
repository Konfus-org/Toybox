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
        set_diffuse_map(Handle());
        set_normal_map(Handle());
        set_specular_map(Handle());
        set_shininess_map(Handle());
        set_emissive_map(Handle());
        set_occlusion_map(Handle());
        set_color(Color(1.0f, 1.0f, 1.0f, 1.0f));
        set_diffuse_map_strength(1.0f);
        set_normal_map_strength(1.0f);
        set_specular(0.5f);
        set_specular_map_strength(1.0f);
        set_shininess(32.0f);
        set_shininess_map_strength(1.0f);
        set_emissive(Color(0.0f, 0.0f, 0.0f, 1.0f));
        set_emissive_map_strength(1.0f);
        set_occlusion(1.0f);
        set_occlusion_map_strength(1.0f);
        set_alpha_cutoff(0.1f);
        set_transparency_amount(0.0f);
        set_exposure(1.0f);
    }

    StandardMaterialInstance::StandardMaterialInstance(
        const Color color,
        const Color emissive,
        const float specular,
        const float shininess,
        const float occlusion,
        const float alpha_cutoff,
        const float diffuse_map_strength,
        const float normal_map_strength,
        const float specular_map_strength,
        const float shininess_map_strength,
        const float emissive_map_strength,
        const float occlusion_map_strength,
        const Handle& diffuse_map,
        const Handle& normal_map,
        const Handle& specular_map,
        const Handle& shininess_map,
        const Handle& emissive_map,
        const Handle& occlusion_map,
        const Handle& material_handle,
        const float transparency_amount)
        : StandardMaterialInstance()
    {
        set_color(color);
        set_emissive(emissive);
        set_diffuse_map_strength(diffuse_map_strength);
        set_normal_map_strength(normal_map_strength);
        set_specular(specular);
        set_specular_map_strength(specular_map_strength);
        set_shininess(shininess);
        set_shininess_map_strength(shininess_map_strength);
        set_emissive_map_strength(emissive_map_strength);
        set_occlusion(occlusion);
        set_occlusion_map_strength(occlusion_map_strength);
        set_alpha_cutoff(alpha_cutoff);
        set_diffuse_map(diffuse_map);
        set_normal_map(normal_map);
        set_specular_map(specular_map);
        set_shininess_map(shininess_map);
        set_emissive_map(emissive_map);
        set_occlusion_map(occlusion_map);
        handle = material_handle;
        set_transparency_amount(transparency_amount);
    }

    StandardMaterialInstance::StandardMaterialInstance(const MaterialInstance& other)
        : StandardMaterialInstance()
    {
        set_color(
            try_get_color_or_default(other.parameters, "color", Color(1.0f, 1.0f, 1.0f, 1.0f)));
        set_emissive(
            try_get_color_or_default(other.parameters, "emissive", Color(0.0f, 0.0f, 0.0f, 1.0f)));
        set_diffuse_map_strength(
            try_get_float_or_default(other.parameters, "diffuse_map_strength", 1.0f));
        set_normal_map_strength(
            try_get_float_or_default(other.parameters, "normal_map_strength", 1.0f));
        set_specular(try_get_float_or_default(other.parameters, "specular", 0.5f));
        set_specular_map_strength(
            try_get_float_or_default(other.parameters, "specular_map_strength", 1.0f));
        set_shininess(try_get_float_or_default(other.parameters, "shininess", 32.0f));
        set_shininess_map_strength(
            try_get_float_or_default(other.parameters, "shininess_map_strength", 1.0f));
        set_emissive_map_strength(
            try_get_float_or_default(other.parameters, "emissive_map_strength", 1.0f));
        set_occlusion(try_get_float_or_default(other.parameters, "occlusion", 1.0f));
        set_occlusion_map_strength(
            try_get_float_or_default(other.parameters, "occlusion_map_strength", 1.0f));
        set_alpha_cutoff(try_get_float_or_default(other.parameters, "alpha_cutoff", 0.1f));
        set_transparency_amount(
            try_get_float_or_default(other.parameters, "transparency_amount", 0.0f));
        set_exposure(try_get_float_or_default(other.parameters, "exposure", 1.0f));
        auto* diffuse_map = other.textures.get("diffuse_map");
        if (!diffuse_map)
            diffuse_map = other.textures.get("diffuse");
        auto* normal_map = other.textures.get("normal_map");
        if (!normal_map)
            normal_map = other.textures.get("normal");
        const auto* specular_map = other.textures.get("specular_map");
        const auto* shininess_map = other.textures.get("shininess_map");
        const auto* emissive_map = other.textures.get("emissive_map");
        const auto* occlusion_map = other.textures.get("occlusion_map");
        set_diffuse_map(diffuse_map ? diffuse_map->texture.handle : Handle());
        set_normal_map(normal_map ? normal_map->texture.handle : Handle());
        set_specular_map(specular_map ? specular_map->texture.handle : Handle());
        set_shininess_map(shininess_map ? shininess_map->texture.handle : Handle());
        set_emissive_map(emissive_map ? emissive_map->texture.handle : Handle());
        set_occlusion_map(occlusion_map ? occlusion_map->texture.handle : Handle());
        handle = other.handle;
        has_loaded_defaults = other.has_loaded_defaults;
    }

    void StandardMaterialInstance::set_diffuse_map(Handle value)
    {
        textures.set("diffuse_map", std::move(value));
    }

    Handle StandardMaterialInstance::get_diffuse_map() const
    {
        const auto* texture = textures.get("diffuse_map");
        if (!texture)
            return {};
        return texture->texture.handle;
    }

    void StandardMaterialInstance::set_normal_map(Handle value)
    {
        textures.set("normal_map", std::move(value));
    }

    Handle StandardMaterialInstance::get_normal_map() const
    {
        const auto* texture = textures.get("normal_map");
        if (!texture)
            return {};
        return texture->texture.handle;
    }

    void StandardMaterialInstance::set_specular_map(Handle value)
    {
        textures.set("specular_map", std::move(value));
    }

    Handle StandardMaterialInstance::get_specular_map() const
    {
        const auto* texture = textures.get("specular_map");
        if (!texture)
            return {};
        return texture->texture.handle;
    }

    void StandardMaterialInstance::set_shininess_map(Handle value)
    {
        textures.set("shininess_map", std::move(value));
    }

    Handle StandardMaterialInstance::get_shininess_map() const
    {
        const auto* texture = textures.get("shininess_map");
        if (!texture)
            return {};
        return texture->texture.handle;
    }

    void StandardMaterialInstance::set_emissive_map(Handle value)
    {
        textures.set("emissive_map", std::move(value));
    }

    Handle StandardMaterialInstance::get_emissive_map() const
    {
        const auto* texture = textures.get("emissive_map");
        if (!texture)
            return {};
        return texture->texture.handle;
    }

    void StandardMaterialInstance::set_occlusion_map(Handle value)
    {
        textures.set("occlusion_map", std::move(value));
    }

    Handle StandardMaterialInstance::get_occlusion_map() const
    {
        const auto* texture = textures.get("occlusion_map");
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

    void StandardMaterialInstance::set_diffuse_map_strength(float value)
    {
        parameters.set("diffuse_map_strength", value);
    }

    float StandardMaterialInstance::get_diffuse_map_strength() const
    {
        return try_get_float_or_default(parameters, "diffuse_map_strength", 1.0f);
    }

    void StandardMaterialInstance::set_normal_map_strength(float value)
    {
        parameters.set("normal_map_strength", value);
    }

    float StandardMaterialInstance::get_normal_map_strength() const
    {
        return try_get_float_or_default(parameters, "normal_map_strength", 1.0f);
    }

    void StandardMaterialInstance::set_specular(float value)
    {
        parameters.set("specular", value);
    }

    float StandardMaterialInstance::get_specular() const
    {
        return try_get_float_or_default(parameters, "specular", 0.5f);
    }

    void StandardMaterialInstance::set_specular_map_strength(float value)
    {
        parameters.set("specular_map_strength", value);
    }

    float StandardMaterialInstance::get_specular_map_strength() const
    {
        return try_get_float_or_default(parameters, "specular_map_strength", 1.0f);
    }

    void StandardMaterialInstance::set_shininess(float value)
    {
        parameters.set("shininess", value);
    }

    float StandardMaterialInstance::get_shininess() const
    {
        return try_get_float_or_default(parameters, "shininess", 32.0f);
    }

    void StandardMaterialInstance::set_shininess_map_strength(float value)
    {
        parameters.set("shininess_map_strength", value);
    }

    float StandardMaterialInstance::get_shininess_map_strength() const
    {
        return try_get_float_or_default(parameters, "shininess_map_strength", 1.0f);
    }

    void StandardMaterialInstance::set_emissive(Color value)
    {
        parameters.set("emissive", std::move(value));
    }

    Color StandardMaterialInstance::get_emissive() const
    {
        return try_get_color_or_default(parameters, "emissive", Color(0.0f, 0.0f, 0.0f, 1.0f));
    }

    void StandardMaterialInstance::set_emissive_map_strength(float value)
    {
        parameters.set("emissive_map_strength", value);
    }

    float StandardMaterialInstance::get_emissive_map_strength() const
    {
        return try_get_float_or_default(parameters, "emissive_map_strength", 1.0f);
    }

    void StandardMaterialInstance::set_occlusion(float value)
    {
        parameters.set("occlusion", value);
    }

    float StandardMaterialInstance::get_occlusion() const
    {
        return try_get_float_or_default(parameters, "occlusion", 1.0f);
    }

    void StandardMaterialInstance::set_occlusion_map_strength(float value)
    {
        parameters.set("occlusion_map_strength", value);
    }

    float StandardMaterialInstance::get_occlusion_map_strength() const
    {
        return try_get_float_or_default(parameters, "occlusion_map_strength", 1.0f);
    }

    void StandardMaterialInstance::set_alpha_cutoff(float value)
    {
        parameters.set("alpha_cutoff", value);
    }

    float StandardMaterialInstance::get_alpha_cutoff() const
    {
        return try_get_float_or_default(parameters, "alpha_cutoff", 0.1f);
    }

    void StandardMaterialInstance::set_transparency_amount(float value)
    {
        parameters.set("transparency_amount", value);
    }

    float StandardMaterialInstance::get_transparency_amount() const
    {
        return try_get_float_or_default(parameters, "transparency_amount", 0.0f);
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
