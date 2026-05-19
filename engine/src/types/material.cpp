#include "tbx/systems/debugging/macros.h"
#include "tbx/types/material.h"
#include "tbx/types/components/material_instance.h"
#include <string>
#include <variant>

namespace tbx
{
    static std::optional<std::reference_wrapper<MaterialParameter>> try_get_uniform_by_name(
        std::vector<MaterialParameter>& values,
        const std::string_view name)
    {
        for (auto& value : values)
        {
            if (value.name == name)
                return std::ref(value);
        }

        return std::nullopt;
    }

    static std::optional<std::reference_wrapper<const MaterialParameter>> try_get_uniform_by_name(
        const std::vector<MaterialParameter>& values,
        const std::string_view name)
    {
        for (const auto& value : values)
        {
            if (value.name == name)
                return std::cref(value);
        }

        return std::nullopt;
    }

    static std::optional<std::reference_wrapper<MaterialTextureBinding>> try_get_texture_by_name(
        std::vector<MaterialTextureBinding>& values,
        const std::string_view name)
    {
        for (auto& texture : values)
        {
            if (texture.name == name)
                return std::ref(texture);
        }

        return std::nullopt;
    }

    static std::optional<std::reference_wrapper<const MaterialTextureBinding>> try_get_texture_by_name(
        const std::vector<MaterialTextureBinding>& values,
        const std::string_view name)
    {
        for (const auto& texture : values)
        {
            if (texture.name == name)
                return std::cref(texture);
        }

        return std::nullopt;
    }

    void MaterialParameterBindings::set(std::string_view name, MaterialParameterData value)
    {
        auto parameter = try_get_uniform_by_name(values, name);
        if (parameter.has_value())
        {
            parameter->get().data = std::move(value);
            return;
        }

        values.push_back(MaterialParameter(name, std::move(value)));
    }

    void MaterialParameterBindings::set(MaterialParameter parameter)
    {
        set(parameter.name, std::move(parameter.data));
    }

    void MaterialParameterBindings::set(std::initializer_list<MaterialParameter> parameters)
    {
        for (auto parameter : parameters)
            set(std::move(parameter));
    }

    std::optional<std::reference_wrapper<MaterialParameter>> MaterialParameterBindings::get(
        std::string_view name)
    {
        return try_get_uniform_by_name(values, name);
    }

    std::optional<std::reference_wrapper<const MaterialParameter>> MaterialParameterBindings::get(
        std::string_view name) const
    {
        return try_get_uniform_by_name(values, name);
    }

    bool MaterialParameterBindings::has(std::string_view name) const
    {
        return get(name).has_value();
    }

    void MaterialParameterBindings::remove(std::string_view name)
    {
        for (auto it = values.begin(); it != values.end(); ++it)
        {
            if (it->name != name)
                continue;

            values.erase(it);
            return;
        }
    }

    void MaterialParameterBindings::clear()
    {
        if (values.empty())
            return;

        values.clear();
    }

    MaterialParameterBindings::iterator MaterialParameterBindings::begin()
    {
        return values.begin();
    }

    MaterialParameterBindings::const_iterator MaterialParameterBindings::begin() const
    {
        return values.begin();
    }

    MaterialParameterBindings::const_iterator MaterialParameterBindings::cbegin() const
    {
        return values.cbegin();
    }

    MaterialParameterBindings::iterator MaterialParameterBindings::end()
    {
        return values.end();
    }

    MaterialParameterBindings::const_iterator MaterialParameterBindings::end() const
    {
        return values.end();
    }

    MaterialParameterBindings::const_iterator MaterialParameterBindings::cend() const
    {
        return values.cend();
    }

    void MaterialTextureBindings::set(std::string_view name, Handle texture)
    {
        auto entry = try_get_texture_by_name(values, name);
        if (entry.has_value())
        {
            entry->get().texture = std::move(texture);
            return;
        }

        values.push_back(
            MaterialTextureBinding {
                .name = std::string(name),
                .texture = std::move(texture),
            });
    }

    void MaterialTextureBindings::set(MaterialTextureBinding texture_binding)
    {
        set(texture_binding.name, std::move(texture_binding.texture));
    }

    void MaterialTextureBindings::set(
        std::initializer_list<MaterialTextureBinding> texture_bindings)
    {
        for (auto texture_binding : texture_bindings)
            set(std::move(texture_binding));
    }

    std::optional<std::reference_wrapper<MaterialTextureBinding>> MaterialTextureBindings::get(
        std::string_view name)
    {
        return try_get_texture_by_name(values, name);
    }

    std::optional<std::reference_wrapper<const MaterialTextureBinding>> MaterialTextureBindings::get(
        std::string_view name) const
    {
        return try_get_texture_by_name(values, name);
    }

    bool MaterialTextureBindings::has(std::string_view name) const
    {
        return get(name).has_value();
    }

    void MaterialTextureBindings::remove(std::string_view name)
    {
        for (auto it = values.begin(); it != values.end(); ++it)
        {
            if (it->name != name)
                continue;

            values.erase(it);
            return;
        }
    }

    void MaterialTextureBindings::clear()
    {
        if (values.empty())
            return;

        values.clear();
    }

    MaterialTextureBindings::iterator MaterialTextureBindings::begin()
    {
        return values.begin();
    }

    MaterialTextureBindings::const_iterator MaterialTextureBindings::begin() const
    {
        return values.begin();
    }

    MaterialTextureBindings::const_iterator MaterialTextureBindings::cbegin() const
    {
        return values.cbegin();
    }

    MaterialTextureBindings::iterator MaterialTextureBindings::end()
    {
        return values.end();
    }

    MaterialTextureBindings::const_iterator MaterialTextureBindings::end() const
    {
        return values.end();
    }

    MaterialTextureBindings::const_iterator MaterialTextureBindings::cend() const
    {
        return values.cend();
    }

    MaterialInstance::MaterialInstance() = default;

    MaterialInstance::MaterialInstance(Handle handle)
        : material(std::move(handle))
    {
    }

    MaterialInstance::MaterialInstance(
        Handle handle,
        MaterialParameterBindings parameter_overrides,
        MaterialTextureBindings texture_overrides_value,
        MaterialConfig config_override,
        const bool has_config_override)
        : material(std::move(handle))
        , texture_overrides(std::move(texture_overrides_value))
        , param_overrides(std::move(parameter_overrides))
        , config(std::move(config_override))
        , _has_config_override(has_config_override)
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

    bool MaterialInstance::has_config_override_enabled() const
    {
        return _has_config_override;
    }

    void MaterialInstance::set_config(MaterialConfig config_override)
    {
        config = std::move(config_override);
        _has_config_override = true;
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

    bool MaterialInstance::get_bool_parameter_or(const std::string_view name, const bool fallback)
        const
    {
        return get_parameter_or(name, fallback);
    }

    int MaterialInstance::get_int_parameter_or(const std::string_view name, const int fallback)
        const
    {
        return get_parameter_or(name, fallback);
    }

    float MaterialInstance::get_float_parameter_or(
        const std::string_view name,
        const float fallback) const
    {
        const auto parameter = param_overrides.get(name);
        if (!parameter.has_value())
            return fallback;
        const auto& data = parameter->get().data;
        if (std::holds_alternative<float>(data))
            return std::get<float>(data);
        if (std::holds_alternative<double>(data))
            return static_cast<float>(std::get<double>(data));
        if (std::holds_alternative<int>(data))
            return static_cast<float>(std::get<int>(data));
        return fallback;
    }

    double MaterialInstance::get_double_parameter_or(
        const std::string_view name,
        const double fallback) const
    {
        const auto parameter = param_overrides.get(name);
        if (!parameter.has_value())
            return fallback;
        const auto& data = parameter->get().data;
        if (std::holds_alternative<double>(data))
            return std::get<double>(data);
        if (std::holds_alternative<float>(data))
            return static_cast<double>(std::get<float>(data));
        if (std::holds_alternative<int>(data))
            return static_cast<double>(std::get<int>(data));
        return fallback;
    }

    Handle MaterialInstance::get_texture_handle_or(
        const std::string_view name,
        const Handle& fallback) const
    {
        const auto texture = texture_overrides.get(name);
        if (!texture.has_value())
            return fallback;
        return texture->get().texture;
    }
}
