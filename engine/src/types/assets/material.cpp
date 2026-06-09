#include "tbx/types/assets/material.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/types/components/material_instance.h"

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

    static std::optional<std::reference_wrapper<const MaterialTextureBinding>>
    try_get_texture_by_name(
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

    MaterialParameter::MaterialParameter(
        std::string parameter_name,
        MaterialParameterData parameter_data)
        : name(std::move(parameter_name))
        , data(std::move(parameter_data))
    {
    }

    MaterialTextureBinding::MaterialTextureBinding(std::string binding_name, Handle texture_handle)
        : name(std::move(binding_name))
        , texture(std::move(texture_handle))
    {
    }

    MaterialTextureBinding::MaterialTextureBinding(const char* binding_name, Handle texture_handle)
        : MaterialTextureBinding(std::string_view(binding_name), std::move(texture_handle))
    {
    }

    MaterialTextureBinding::MaterialTextureBinding(
        std::string_view binding_name,
        Handle texture_handle)
        : MaterialTextureBinding(std::string(binding_name), std::move(texture_handle))
    {
    }

    void MaterialParameterBindings::set(std::string_view name, MaterialParameterData value)
    {
        set(MaterialParameter(name, std::move(value)));
    }

    void MaterialParameterBindings::set(MaterialParameter parameter)
    {
        auto existing_parameter = try_get_uniform_by_name(values, parameter.name);
        if (existing_parameter.has_value())
        {
            existing_parameter->get().name = std::move(parameter.name);
            existing_parameter->get().data = std::move(parameter.data);
            return;
        }

        values.push_back(std::move(parameter));
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
        set(MaterialTextureBinding(name, std::move(texture)));
    }

    void MaterialTextureBindings::set(MaterialTextureBinding texture_binding)
    {
        auto existing_texture = try_get_texture_by_name(values, texture_binding.name);
        if (existing_texture.has_value())
        {
            existing_texture->get().name = std::move(texture_binding.name);
            existing_texture->get().texture = std::move(texture_binding.texture);
            return;
        }

        values.push_back(std::move(texture_binding));
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

    std::optional<std::reference_wrapper<const MaterialTextureBinding>> MaterialTextureBindings::
        get(std::string_view name) const
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

    MaterialInstance::MaterialInstance(Handle handle, MaterialOverrides material_overrides)
        : material(std::move(handle))
        , overrides(std::move(material_overrides))
    {
        overrides.has_texture_override =
            overrides.has_texture_override || !overrides.textures.values.empty();
        overrides.has_parameter_override =
            overrides.has_parameter_override || !overrides.parameters.values.empty();
    }

    MaterialInstance::MaterialInstance(
        Handle handle,
        MaterialParameterBindings parameter_overrides,
        MaterialTextureBindings texture_overrides_value,
        MaterialConfig config_override,
        const bool has_config_override)
        : material(std::move(handle))
    {
        overrides.textures = std::move(texture_overrides_value);
        overrides.parameters = std::move(parameter_overrides);
        overrides.config = std::move(config_override);
        overrides.has_texture_override = !overrides.textures.values.empty();
        overrides.has_parameter_override = !overrides.parameters.values.empty();
        overrides.has_config_override = has_config_override;
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
        return overrides.has_config_override;
    }

    void MaterialInstance::set_config(MaterialConfig config_override)
    {
        overrides.config = std::move(config_override);
        overrides.has_config_override = true;
        mark_dirty();
    }

    void MaterialInstance::set_parameter(const std::string& name, MaterialParameterData value)
    {
        overrides.parameters.set(name, std::move(value));
        overrides.has_parameter_override = true;
        mark_dirty();
    }

    void MaterialInstance::set_texture(const std::string& name, Handle texture)
    {
        overrides.textures.set(name, std::move(texture));
        overrides.has_texture_override = true;
        mark_dirty();
    }

    void MaterialInstance::set_bool(const std::string& name, const bool value)
    {
        set_parameter(name, value);
    }

    void MaterialInstance::set_int(const std::string& name, const int value)
    {
        set_parameter(name, value);
    }

    void MaterialInstance::set_float(const std::string& name, const float value)
    {
        set_parameter(name, value);
    }

    void MaterialInstance::set_double(const std::string& name, const double value)
    {
        set_parameter(name, value);
    }

    void MaterialInstance::set_vec2(const std::string& name, const Vec2& value)
    {
        set_parameter(name, value);
    }

    void MaterialInstance::set_vec3(const std::string& name, const Vec3& value)
    {
        set_parameter(name, value);
    }

    void MaterialInstance::set_vec4(const std::string& name, const Vec4& value)
    {
        set_parameter(name, value);
    }

    void MaterialInstance::set_color(const std::string& name, const Color& value)
    {
        set_parameter(name, value);
    }

    void MaterialInstance::set_mat3(const std::string& name, const Mat3& value)
    {
        set_parameter(name, value);
    }

    void MaterialInstance::set_mat4(const std::string& name, const Mat4& value)
    {
        set_parameter(name, value);
    }

    bool MaterialInstance::get_bool_parameter_or(const std::string& name, const bool fallback) const
    {
        return get_parameter_or(name, fallback);
    }

    int MaterialInstance::get_int_parameter_or(const std::string& name, const int fallback) const
    {
        return get_parameter_or(name, fallback);
    }

    float MaterialInstance::get_float_parameter_or(const std::string& name, const float fallback)
        const
    {
        const auto parameter = overrides.parameters.get(name);
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

    double MaterialInstance::get_double_parameter_or(const std::string& name, const double fallback)
        const
    {
        const auto parameter = overrides.parameters.get(name);
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

    Handle MaterialInstance::get_texture_handle_or(const std::string& name, const Handle& fallback)
        const
    {
        const auto texture = overrides.textures.get(name);
        if (!texture.has_value())
            return fallback;
        return texture->get().texture;
    }

}
