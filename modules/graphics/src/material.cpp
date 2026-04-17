#include "tbx/graphics/material.h"
#include <variant>

namespace tbx
{
    MaterialInstance::MaterialInstance() = default;

    MaterialInstance::MaterialInstance(Handle handle)
        : material(std::move(handle))
    {
    }

    MaterialInstance::MaterialInstance(
        Handle handle,
        ParamBindings parameter_overrides,
        TextureBindings texture_overrides_value,
        Depth depth_override,
        const bool has_depth_override)
        : material(std::move(handle))
        , texture_overrides(std::move(texture_overrides_value))
        , param_overrides(std::move(parameter_overrides))
        , depth(std::move(depth_override))
        , _has_depth_override(has_depth_override)
    {
    }

    bool MaterialInstance::is_dirty() const
    {
        return _is_dirty;
    }

    void MaterialInstance::clear_dirty()
    {
        _is_dirty = false;
    }

    void MaterialInstance::mark_dirty()
    {
        _is_dirty = true;
    }

    const Handle& MaterialInstance::get_handle() const
    {
        return material;
    }

    bool MaterialInstance::has_depth_override_enabled() const
    {
        return _has_depth_override;
    }

    void MaterialInstance::set_depth(Depth depth_override)
    {
        depth = std::move(depth_override);
        _has_depth_override = true;
        mark_dirty();
    }

    void MaterialInstance::set_parameter(std::string_view name, MaterialParameterData value)
    {
        param_overrides.set(name, std::move(value));
        mark_dirty();
    }

    void MaterialInstance::set_texture(std::string_view name, Handle texture)
    {
        texture_overrides.set(name, std::move(texture));
        mark_dirty();
    }

    void MaterialInstance::set_texture(std::string_view name, TextureInstance texture)
    {
        texture_overrides.set(name, std::move(texture));
        mark_dirty();
    }

    bool MaterialInstance::get_bool_parameter_or(const std::string_view name, const bool fallback) const
    {
        return get_parameter_or(name, fallback);
    }

    int MaterialInstance::get_int_parameter_or(const std::string_view name, const int fallback) const
    {
        return get_parameter_or(name, fallback);
    }

    float MaterialInstance::get_float_parameter_or(
        const std::string_view name,
        const float fallback) const
    {
        const auto* parameter = param_overrides.get(name);
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

    double MaterialInstance::get_double_parameter_or(
        const std::string_view name,
        const double fallback) const
    {
        const auto* parameter = param_overrides.get(name);
        if (!parameter)
            return fallback;
        if (const auto* value = std::get_if<double>(&parameter->data))
            return *value;
        if (const auto* value = std::get_if<float>(&parameter->data))
            return static_cast<double>(*value);
        if (const auto* value = std::get_if<int>(&parameter->data))
            return static_cast<double>(*value);
        return fallback;
    }

    Handle MaterialInstance::get_texture_handle_or(
        const std::string_view name,
        const Handle& fallback) const
    {
        const auto* texture = texture_overrides.get(name);
        if (!texture)
            return fallback;
        return texture->texture.handle;
    }
}
