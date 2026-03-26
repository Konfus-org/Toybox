#include "tbx/graphics/material.h"
#include <variant>

namespace tbx
{
    static Handle get_builtin_unlit_material_handle()
    {
        return Handle(Uuid(0x00000008U));
    }

    static Color try_get_color_or_default(
        const MaterialParameterBindings& parameters,
        const std::string_view name,
        const Color& fallback)
    {
        const auto* parameter = parameters.get(name);
        if (!parameter)
            return fallback;
        if (const auto* color = std::get_if<Color>(&parameter->data))
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
        if (const auto* value = std::get_if<float>(&parameter->data))
            return *value;
        if (const auto* value = std::get_if<double>(&parameter->data))
            return static_cast<float>(*value);
        if (const auto* value = std::get_if<int>(&parameter->data))
            return static_cast<float>(*value);
        return fallback;
    }

    PbrMaterialInstance::PbrMaterialInstance()
    {
        set_diffuse_map(Handle());
        set_normal_map(Handle());
        set_specular_map(Handle());
        set_shininess_map(Handle());
        set_emissive_map(Handle());
        set_color(Color(1.0f, 1.0f, 1.0f, 1.0f));
        set_diffuse_strength(1.0f);
        set_normal_strength(1.0f);
        set_specular_strength(0.5f);
        set_shininess_strength(32.0f);
        set_emissive_color(Color(0.0f, 0.0f, 0.0f, 1.0f));
        set_emissive_strength(1.0f);
        set_alpha_cutoff(0.1f);
        set_transparency_amount(0.0f);
        set_exposure(1.0f);
    }

    PbrMaterialInstance::PbrMaterialInstance(
        const Color color,
        const Color emissive_color,
        const float specular_strength,
        const float shininess_strength,
        const float alpha_cutoff,
        const float diffuse_strength,
        const float normal_strength,
        const float emissive_strength,
        const Handle& diffuse_map,
        const Handle& normal_map,
        const Handle& specular_map,
        const Handle& shininess_map,
        const Handle& emissive_map,
        const Handle& material_handle,
        const float transparency_amount)
        : PbrMaterialInstance()
    {
        set_color(color);
        set_emissive_color(emissive_color);
        set_diffuse_strength(diffuse_strength);
        set_normal_strength(normal_strength);
        set_specular_strength(specular_strength);
        set_shininess_strength(shininess_strength);
        set_emissive_strength(emissive_strength);
        set_alpha_cutoff(alpha_cutoff);
        set_diffuse_map(diffuse_map);
        set_normal_map(normal_map);
        set_specular_map(specular_map);
        set_shininess_map(shininess_map);
        set_emissive_map(emissive_map);
        handle = material_handle;
        set_transparency_amount(transparency_amount);
    }

    PbrMaterialInstance::PbrMaterialInstance(const MaterialInstance& other)
        : PbrMaterialInstance()
    {
        set_color(
            try_get_color_or_default(other.parameters, "color", Color(1.0f, 1.0f, 1.0f, 1.0f)));
        set_emissive_color(
            try_get_color_or_default(other.parameters, "emissive", Color(0.0f, 0.0f, 0.0f, 1.0f)));
        set_diffuse_strength(try_get_float_or_default(other.parameters, "diffuse_strength", 1.0f));
        set_normal_strength(try_get_float_or_default(other.parameters, "normal_strength", 1.0f));
        set_specular_strength(
            try_get_float_or_default(other.parameters, "specular_strength", 0.5f));
        set_shininess_strength(
            try_get_float_or_default(other.parameters, "shininess_strength", 32.0f));
        set_emissive_strength(
            try_get_float_or_default(other.parameters, "emissive_strength", 1.0f));
        set_alpha_cutoff(try_get_float_or_default(other.parameters, "alpha_cutoff", 0.1f));
        set_transparency_amount(
            try_get_float_or_default(other.parameters, "transparency_amount", 0.0f));
        set_exposure(try_get_float_or_default(other.parameters, "exposure", 1.0f));
        const auto* diffuse_map = other.textures.get("diffuse_map");
        const auto* normal_map = other.textures.get("normal_map");
        const auto* specular_map = other.textures.get("specular_map");
        const auto* shininess_map = other.textures.get("shininess_map");
        const auto* emissive_map = other.textures.get("emissive_map");
        set_diffuse_map(diffuse_map ? diffuse_map->texture.handle : Handle());
        set_normal_map(normal_map ? normal_map->texture.handle : Handle());
        set_specular_map(specular_map ? specular_map->texture.handle : Handle());
        set_shininess_map(shininess_map ? shininess_map->texture.handle : Handle());
        set_emissive_map(emissive_map ? emissive_map->texture.handle : Handle());
        handle = other.handle;
        has_loaded_defaults = other.has_loaded_defaults;
    }

    void PbrMaterialInstance::set_diffuse_map(Handle value)
    {
        textures.set("diffuse_map", std::move(value));
    }

    Handle PbrMaterialInstance::get_diffuse_map() const
    {
        const auto* texture = textures.get("diffuse_map");
        if (!texture)
            return {};
        return texture->texture.handle;
    }

    void PbrMaterialInstance::set_normal_map(Handle value)
    {
        textures.set("normal_map", std::move(value));
    }

    Handle PbrMaterialInstance::get_normal_map() const
    {
        const auto* texture = textures.get("normal_map");
        if (!texture)
            return {};
        return texture->texture.handle;
    }

    void PbrMaterialInstance::set_specular_map(Handle value)
    {
        textures.set("specular_map", std::move(value));
    }

    Handle PbrMaterialInstance::get_specular_map() const
    {
        const auto* texture = textures.get("specular_map");
        if (!texture)
            return {};
        return texture->texture.handle;
    }

    void PbrMaterialInstance::set_shininess_map(Handle value)
    {
        textures.set("shininess_map", std::move(value));
    }

    Handle PbrMaterialInstance::get_shininess_map() const
    {
        const auto* texture = textures.get("shininess_map");
        if (!texture)
            return {};
        return texture->texture.handle;
    }

    void PbrMaterialInstance::set_emissive_map(Handle value)
    {
        textures.set("emissive_map", std::move(value));
    }

    Handle PbrMaterialInstance::get_emissive_map() const
    {
        const auto* texture = textures.get("emissive_map");
        if (!texture)
            return {};
        return texture->texture.handle;
    }

    void PbrMaterialInstance::set_color(Color value)
    {
        parameters.set("color", std::move(value));
    }

    Color PbrMaterialInstance::get_color() const
    {
        return try_get_color_or_default(parameters, "color", Color(1.0f, 1.0f, 1.0f, 1.0f));
    }

    void PbrMaterialInstance::set_diffuse_strength(float value)
    {
        parameters.set("diffuse_strength", value);
    }

    float PbrMaterialInstance::get_diffuse_strength() const
    {
        return try_get_float_or_default(parameters, "diffuse_strength", 1.0f);
    }

    void PbrMaterialInstance::set_normal_strength(float value)
    {
        parameters.set("normal_strength", value);
    }

    float PbrMaterialInstance::get_normal_strength() const
    {
        return try_get_float_or_default(parameters, "normal_strength", 1.0f);
    }

    void PbrMaterialInstance::set_specular_strength(float value)
    {
        parameters.set("specular_strength", value);
    }

    float PbrMaterialInstance::get_specular_strength() const
    {
        return try_get_float_or_default(parameters, "specular_strength", 0.5f);
    }

    void PbrMaterialInstance::set_shininess_strength(float value)
    {
        parameters.set("shininess_strength", value);
    }

    float PbrMaterialInstance::get_shininess_strength() const
    {
        return try_get_float_or_default(parameters, "shininess_strength", 32.0f);
    }

    void PbrMaterialInstance::set_emissive_color(Color value)
    {
        parameters.set("emissive", std::move(value));
    }

    Color PbrMaterialInstance::get_emissive_color() const
    {
        return try_get_color_or_default(parameters, "emissive", Color(0.0f, 0.0f, 0.0f, 1.0f));
    }

    void PbrMaterialInstance::set_emissive_strength(float value)
    {
        parameters.set("emissive_strength", value);
    }

    float PbrMaterialInstance::get_emissive_strength() const
    {
        return try_get_float_or_default(parameters, "emissive_strength", 1.0f);
    }

    void PbrMaterialInstance::set_alpha_cutoff(float value)
    {
        parameters.set("alpha_cutoff", value);
    }

    float PbrMaterialInstance::get_alpha_cutoff() const
    {
        return try_get_float_or_default(parameters, "alpha_cutoff", 0.1f);
    }

    void PbrMaterialInstance::set_transparency_amount(float value)
    {
        parameters.set("transparency_amount", value);
    }

    float PbrMaterialInstance::get_transparency_amount() const
    {
        return try_get_float_or_default(parameters, "transparency_amount", 0.0f);
    }

    void PbrMaterialInstance::set_exposure(float value)
    {
        parameters.set("exposure", value);
    }

    float PbrMaterialInstance::get_exposure() const
    {
        return try_get_float_or_default(parameters, "exposure", 1.0f);
    }

    FlatMaterialInstance::FlatMaterialInstance()
    {
        handle = get_builtin_unlit_material_handle();
        set_diffuse_map(Handle());
        set_color(Color(1.0f, 1.0f, 1.0f, 1.0f));
        set_emissive_color(Color(0.0f, 0.0f, 0.0f, 1.0f));
        set_alpha_cutoff(0.1f);
        set_transparency_amount(0.0f);
        set_exposure(1.0f);
    }

    FlatMaterialInstance::FlatMaterialInstance(
        const Color color,
        const Color emissive_color,
        const float alpha_cutoff,
        const Handle& diffuse_map,
        const Handle& material_handle,
        const float transparency_amount,
        const float exposure)
        : FlatMaterialInstance()
    {
        set_color(color);
        set_emissive_color(emissive_color);
        set_alpha_cutoff(alpha_cutoff);
        set_diffuse_map(diffuse_map);
        handle = material_handle.is_valid() ? material_handle : get_builtin_unlit_material_handle();
        set_transparency_amount(transparency_amount);
        set_exposure(exposure);
    }

    FlatMaterialInstance::FlatMaterialInstance(const MaterialInstance& other)
        : FlatMaterialInstance()
    {
        set_color(
            try_get_color_or_default(other.parameters, "color", Color(1.0f, 1.0f, 1.0f, 1.0f)));
        set_emissive_color(
            try_get_color_or_default(other.parameters, "emissive", Color(0.0f, 0.0f, 0.0f, 1.0f)));
        set_alpha_cutoff(try_get_float_or_default(other.parameters, "alpha_cutoff", 0.1f));
        set_transparency_amount(
            try_get_float_or_default(other.parameters, "transparency_amount", 0.0f));
        set_exposure(try_get_float_or_default(other.parameters, "exposure", 1.0f));
        const auto* diffuse_map = other.textures.get("diffuse_map");
        set_diffuse_map(diffuse_map ? diffuse_map->texture.handle : Handle());
        handle = other.handle.is_valid() ? other.handle : get_builtin_unlit_material_handle();
        has_loaded_defaults = other.has_loaded_defaults;
    }

    void FlatMaterialInstance::set_diffuse_map(Handle value)
    {
        textures.set("diffuse_map", std::move(value));
    }

    Handle FlatMaterialInstance::get_diffuse_map() const
    {
        const auto* texture = textures.get("diffuse_map");
        if (!texture)
            return {};
        return texture->texture.handle;
    }

    void FlatMaterialInstance::set_color(Color value)
    {
        parameters.set("color", std::move(value));
    }

    Color FlatMaterialInstance::get_color() const
    {
        return try_get_color_or_default(parameters, "color", Color(1.0f, 1.0f, 1.0f, 1.0f));
    }

    void FlatMaterialInstance::set_emissive_color(Color value)
    {
        parameters.set("emissive", std::move(value));
    }

    Color FlatMaterialInstance::get_emissive_color() const
    {
        return try_get_color_or_default(parameters, "emissive", Color(0.0f, 0.0f, 0.0f, 1.0f));
    }

    void FlatMaterialInstance::set_alpha_cutoff(float value)
    {
        parameters.set("alpha_cutoff", value);
    }

    float FlatMaterialInstance::get_alpha_cutoff() const
    {
        return try_get_float_or_default(parameters, "alpha_cutoff", 0.1f);
    }

    void FlatMaterialInstance::set_transparency_amount(float value)
    {
        parameters.set("transparency_amount", value);
    }

    float FlatMaterialInstance::get_transparency_amount() const
    {
        return try_get_float_or_default(parameters, "transparency_amount", 0.0f);
    }

    void FlatMaterialInstance::set_exposure(float value)
    {
        parameters.set("exposure", value);
    }

    float FlatMaterialInstance::get_exposure() const
    {
        return try_get_float_or_default(parameters, "exposure", 1.0f);
    }

}
