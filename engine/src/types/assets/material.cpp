#include "tbx/types/assets/material.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/types/components/material_instance.h"
#include "types/assets/internal/material_internal.h"
#include <string>
#include <type_traits>
#include <variant>

namespace tbx
{
    MaterialParameter::MaterialParameter(uint32 parameter_id, MaterialParameterData parameter_data)
        : id(parameter_id)
        , data(std::move(parameter_data))
    {
    }

    MaterialTextureBinding::MaterialTextureBinding(uint32 binding_id, Handle texture_handle)
        : id(binding_id)
        , texture(std::move(texture_handle))
    {
    }

    MaterialTextureBinding::MaterialTextureBinding(
        const std::string& binding_name,
        Handle texture_handle)
        : MaterialTextureBinding(std::string_view(binding_name), std::move(texture_handle))
    {
    }

    MaterialTextureBinding::MaterialTextureBinding(
        std::string_view binding_name,
        Handle texture_handle)
        : name(binding_name)
        , id(make_param_id(binding_name))
        , texture(std::move(texture_handle))
    {
    }

    void MaterialParameterBindings::set(std::string_view name, MaterialParameterData value)
    {
        set(MaterialParameter(name, std::move(value)));
    }

    void MaterialParameterBindings::set(const uint32 id, MaterialParameterData value)
    {
        auto parameter = internal::try_get_uniform_by_id(values, id);
        if (parameter.has_value())
        {
            parameter->get().data = std::move(value);
            return;
        }

        values.push_back(MaterialParameter(id, std::move(value)));
    }

    void MaterialParameterBindings::set(MaterialParameter parameter)
    {
        const uint32 id = parameter.id == INVALID_MATERIAL_PARAM_ID && !parameter.name.empty()
                              ? make_param_id(parameter.name)
                              : parameter.id;
        auto existing_parameter = internal::try_get_uniform_by_id(values, id);
        if (existing_parameter.has_value())
        {
            existing_parameter->get().name = std::move(parameter.name);
            existing_parameter->get().data = std::move(parameter.data);
            return;
        }

        parameter.id = id;
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
        return get(make_param_id(name));
    }

    std::optional<std::reference_wrapper<const MaterialParameter>> MaterialParameterBindings::get(
        std::string_view name) const
    {
        return get(make_param_id(name));
    }

    std::optional<std::reference_wrapper<MaterialParameter>> MaterialParameterBindings::get(
        const uint32 id)
    {
        return internal::try_get_uniform_by_id(values, id);
    }

    std::optional<std::reference_wrapper<const MaterialParameter>> MaterialParameterBindings::get(
        const uint32 id) const
    {
        return internal::try_get_uniform_by_id(values, id);
    }

    bool MaterialParameterBindings::has(std::string_view name) const
    {
        return has(make_param_id(name));
    }

    bool MaterialParameterBindings::has(const uint32 id) const
    {
        return get(id).has_value();
    }

    void MaterialParameterBindings::remove(std::string_view name)
    {
        remove(make_param_id(name));
    }

    void MaterialParameterBindings::remove(const uint32 id)
    {
        for (auto it = values.begin(); it != values.end(); ++it)
        {
            if (it->id != id)
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

    void MaterialTextureBindings::set(const uint32 id, Handle texture)
    {
        auto entry = internal::try_get_texture_by_id(values, id);
        if (entry.has_value())
        {
            entry->get().texture = std::move(texture);
            return;
        }

        values.push_back(MaterialTextureBinding(id, std::move(texture)));
    }

    void MaterialTextureBindings::set(MaterialTextureBinding texture_binding)
    {
        const uint32 id =
            texture_binding.id == INVALID_MATERIAL_PARAM_ID && !texture_binding.name.empty()
                ? make_param_id(texture_binding.name)
                : texture_binding.id;
        auto existing_texture = internal::try_get_texture_by_id(values, id);
        if (existing_texture.has_value())
        {
            existing_texture->get().name = std::move(texture_binding.name);
            existing_texture->get().texture = std::move(texture_binding.texture);
            return;
        }

        texture_binding.id = id;
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
        return get(make_param_id(name));
    }

    std::optional<std::reference_wrapper<const MaterialTextureBinding>> MaterialTextureBindings::
        get(std::string_view name) const
    {
        return get(make_param_id(name));
    }

    std::optional<std::reference_wrapper<MaterialTextureBinding>> MaterialTextureBindings::get(
        const uint32 id)
    {
        return internal::try_get_texture_by_id(values, id);
    }

    std::optional<std::reference_wrapper<const MaterialTextureBinding>> MaterialTextureBindings::
        get(const uint32 id) const
    {
        return internal::try_get_texture_by_id(values, id);
    }

    bool MaterialTextureBindings::has(std::string_view name) const
    {
        return has(make_param_id(name));
    }

    bool MaterialTextureBindings::has(const uint32 id) const
    {
        return get(id).has_value();
    }

    void MaterialTextureBindings::remove(std::string_view name)
    {
        remove(make_param_id(name));
    }

    void MaterialTextureBindings::remove(const uint32 id)
    {
        for (auto it = values.begin(); it != values.end(); ++it)
        {
            if (it->id != id)
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
        set_parameter(make_param_id(name), std::move(value));
    }

    void MaterialInstance::set_parameter(const uint32 id, MaterialParameterData value)
    {
        overrides.parameters.set(id, std::move(value));
        overrides.has_parameter_override = true;
        mark_dirty();
    }

    void MaterialInstance::set_texture(const std::string& name, Handle texture)
    {
        set_texture(make_param_id(name), std::move(texture));
    }

    void MaterialInstance::set_texture(const uint32 id, Handle texture)
    {
        overrides.textures.set(id, std::move(texture));
        overrides.has_texture_override = true;
        mark_dirty();
    }

    void MaterialInstance::set_bool(const uint32 id, const bool value)
    {
        set_parameter(id, value);
    }

    void MaterialInstance::set_int(const uint32 id, const int value)
    {
        set_parameter(id, value);
    }

    void MaterialInstance::set_float(const uint32 id, const float value)
    {
        set_parameter(id, value);
    }

    void MaterialInstance::set_double(const uint32 id, const double value)
    {
        set_parameter(id, value);
    }

    void MaterialInstance::set_vec2(const uint32 id, const Vec2& value)
    {
        set_parameter(id, value);
    }

    void MaterialInstance::set_vec3(const uint32 id, const Vec3& value)
    {
        set_parameter(id, value);
    }

    void MaterialInstance::set_vec4(const uint32 id, const Vec4& value)
    {
        set_parameter(id, value);
    }

    void MaterialInstance::set_color(const uint32 id, const Color& value)
    {
        set_parameter(id, value);
    }

    void MaterialInstance::set_mat3(const uint32 id, const Mat3& value)
    {
        set_parameter(id, value);
    }

    void MaterialInstance::set_mat4(const uint32 id, const Mat4& value)
    {
        set_parameter(id, value);
    }

    bool MaterialInstance::get_bool_parameter_or(const std::string& name, const bool fallback) const
    {
        return get_bool_parameter_or(make_param_id(name), fallback);
    }

    bool MaterialInstance::get_bool_parameter_or(const uint32 id, const bool fallback) const
    {
        return get_parameter_or(id, fallback);
    }

    int MaterialInstance::get_int_parameter_or(const std::string& name, const int fallback) const
    {
        return get_int_parameter_or(make_param_id(name), fallback);
    }

    int MaterialInstance::get_int_parameter_or(const uint32 id, const int fallback) const
    {
        return get_parameter_or(id, fallback);
    }

    float MaterialInstance::get_float_parameter_or(const std::string& name, const float fallback)
        const
    {
        return get_float_parameter_or(make_param_id(name), fallback);
    }

    float MaterialInstance::get_float_parameter_or(const uint32 id, const float fallback) const
    {
        const auto parameter = overrides.parameters.get(id);
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
        return get_double_parameter_or(make_param_id(name), fallback);
    }

    double MaterialInstance::get_double_parameter_or(const uint32 id, const double fallback) const
    {
        const auto parameter = overrides.parameters.get(id);
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
        return get_texture_handle_or(make_param_id(name), fallback);
    }

    Handle MaterialInstance::get_texture_handle_or(const uint32 id, const Handle& fallback) const
    {
        const auto texture = overrides.textures.get(id);
        if (!texture.has_value())
            return fallback;
        return texture->get().texture;
    }

    static uint64 hash_material_parameter_data(
        const MaterialParameterData& data,
        const uint64 value)
    {
        uint64 result = hash(static_cast<uint64>(data.index()), value);
        std::visit(
            [&result](const auto& parameter_value)
            {
                using TValue = std::decay_t<decltype(parameter_value)>;
                if constexpr (std::is_same_v<TValue, bool>)
                    result = hash(static_cast<uint64>(parameter_value ? 1U : 0U), result);
                else
                    result = hash(parameter_value, result);
            },
            data);
        return result;
    }

    static uint64 hash_material_config(const MaterialConfig& config, const uint64 value)
    {
        uint64 result = value;
        result = hash(static_cast<uint64>(config.is_depth_test_enabled ? 1U : 0U), result);
        result = hash(static_cast<uint64>(config.is_depth_write_enabled ? 1U : 0U), result);
        result = hash(static_cast<uint64>(config.is_depth_prepass_enabled ? 1U : 0U), result);
        result = hash(static_cast<uint64>(config.is_two_sided ? 1U : 0U), result);
        result = hash(static_cast<uint64>(config.is_cullable ? 1U : 0U), result);
        result = hash(static_cast<uint64>(config.depth_function), result);
        result = hash(static_cast<uint64>(config.blend_mode), result);
        return hash(static_cast<uint64>(config.shadow_mode), result);
    }

    static uint64 hash_material_instance(const MaterialInstance& material, const uint64 value)
    {
        uint64 result = hash(material.get_handle().id, value);
        result = hash(material.get_handle().name, result);
        result =
            hash(static_cast<uint64>(material.has_config_override_enabled() ? 1U : 0U), result);
        if (material.has_config_override_enabled())
            result = hash(material.overrides.config, result);

        result =
            hash(static_cast<uint64>(material.overrides.has_parameter_override ? 1U : 0U), result);
        for (const auto& parameter : material.overrides.parameters)
        {
            result = hash(parameter.id, result);
            result = hash(parameter.data, result);
        }

        result =
            hash(static_cast<uint64>(material.overrides.has_texture_override ? 1U : 0U), result);
        for (const auto& texture : material.overrides.textures)
        {
            result = hash(texture.id, result);
            result = hash(texture.texture.id, result);
            result = hash(texture.texture.name, result);
        }

        return result == 0U ? 1U : result;
    }

    uint64 hash(const MaterialParameterData& data, const uint64 value)
    {
        return hash_combine_value(
            value,
            hash_material_parameter_data(data, TBX_FNV1A_OFFSET_BASIS));
    }

    uint64 hash(const MaterialConfig& config, const uint64 value)
    {
        return hash_combine_value(value, hash_material_config(config, TBX_FNV1A_OFFSET_BASIS));
    }

    uint64 hash(const MaterialInstance& material, const uint64 value)
    {
        return hash_combine_value(value, hash_material_instance(material, TBX_FNV1A_OFFSET_BASIS));
    }
}
