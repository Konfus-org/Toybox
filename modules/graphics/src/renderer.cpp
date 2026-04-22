#include "tbx/graphics/renderer.h"
#include "tbx/debugging/macros.h"
#include <string>

namespace tbx
{
    static std::string normalize_uniform_name(std::string_view name)
    {
        if (name.size() >= 2U && name[0] == 'u' && name[1] == '_')
            return std::string(name);

        std::string normalized = "u_";
        normalized.append(name.begin(), name.end());
        return normalized;
    }

    static MaterialParameter* try_get_uniform_by_name(
        std::vector<MaterialParameter>& values,
        const std::string& normalized_name)
    {
        for (auto& value : values)
        {
            if (value.name == normalized_name)
                return &value;
        }

        return nullptr;
    }

    static const MaterialParameter* try_get_uniform_by_name(
        const std::vector<MaterialParameter>& values,
        const std::string& normalized_name)
    {
        for (const auto& value : values)
        {
            if (value.name == normalized_name)
                return &value;
        }

        return nullptr;
    }

    static MaterialTextureBinding* try_get_texture_by_name(
        std::vector<MaterialTextureBinding>& values,
        const std::string& normalized_name)
    {
        for (auto& texture : values)
        {
            if (texture.name == normalized_name)
                return &texture;
        }

        return nullptr;
    }

    static const MaterialTextureBinding* try_get_texture_by_name(
        const std::vector<MaterialTextureBinding>& values,
        const std::string& normalized_name)
    {
        for (const auto& texture : values)
        {
            if (texture.name == normalized_name)
                return &texture;
        }

        return nullptr;
    }

    void MaterialParameterBindings::set(std::string_view name, MaterialParameterData value)
    {
        const std::string normalized_name = normalize_uniform_name(name);

        auto* parameter = try_get_uniform_by_name(values, normalized_name);
        if (parameter)
        {
            parameter->data = std::move(value);
            return;
        }

        values.push_back(MaterialParameter(normalized_name, std::move(value)));
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

    MaterialParameter* MaterialParameterBindings::get(std::string_view name)
    {
        const std::string normalized_name = normalize_uniform_name(name);
        return try_get_uniform_by_name(values, normalized_name);
    }

    const MaterialParameter* MaterialParameterBindings::get(std::string_view name) const
    {
        const std::string normalized_name = normalize_uniform_name(name);
        return try_get_uniform_by_name(values, normalized_name);
    }

    bool MaterialParameterBindings::has(std::string_view name) const
    {
        return get(name) != nullptr;
    }

    void MaterialParameterBindings::remove(std::string_view name)
    {
        const std::string normalized_name = normalize_uniform_name(name);

        for (auto it = values.begin(); it != values.end(); ++it)
        {
            if (it->name != normalized_name)
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
        const std::string normalized_name = normalize_uniform_name(name);
        auto* entry = try_get_texture_by_name(values, normalized_name);
        if (entry)
        {
            entry->texture = std::move(texture);
            return;
        }

        values.push_back(
            MaterialTextureBinding {
                .name = normalized_name,
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

    MaterialTextureBinding* MaterialTextureBindings::get(std::string_view name)
    {
        const std::string normalized_name = normalize_uniform_name(name);
        return try_get_texture_by_name(values, normalized_name);
    }

    const MaterialTextureBinding* MaterialTextureBindings::get(std::string_view name) const
    {
        const std::string normalized_name = normalize_uniform_name(name);
        return try_get_texture_by_name(values, normalized_name);
    }

    bool MaterialTextureBindings::has(std::string_view name) const
    {
        return get(name) != nullptr;
    }

    void MaterialTextureBindings::remove(std::string_view name)
    {
        const std::string normalized_name = normalize_uniform_name(name);
        for (auto it = values.begin(); it != values.end(); ++it)
        {
            if (it->name != normalized_name)
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

}
