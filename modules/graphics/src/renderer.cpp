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

    static MaterialParameterBinding* try_get_uniform_by_name(
        std::vector<MaterialParameterBinding>& values,
        const std::string& normalized_name)
    {
        for (auto& value : values)
        {
            if (value.name == normalized_name)
                return &value;
        }

        return nullptr;
    }

    static const MaterialParameterBinding* try_get_uniform_by_name(
        const std::vector<MaterialParameterBinding>& values,
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
        std::vector<MaterialTextureBinding>& overrides,
        const std::string& normalized_name)
    {
        for (auto& texture : overrides)
        {
            if (texture.name == normalized_name)
                return &texture;
        }

        return nullptr;
    }

    static const MaterialTextureBinding* try_get_texture_by_name(
        const std::vector<MaterialTextureBinding>& overrides,
        const std::string& normalized_name)
    {
        for (const auto& texture : overrides)
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
            parameter->value = std::move(value);
            return;
        }

        values.push_back(
            MaterialParameterBinding {
                .name = normalized_name,
                .value = std::move(value),
            });
    }

    void MaterialParameterBindings::set(MaterialParameterBinding parameter)
    {
        set(parameter.name, std::move(parameter.value));
    }

    void MaterialParameterBindings::set(std::initializer_list<MaterialParameterBinding> parameters)
    {
        for (auto parameter : parameters)
            set(std::move(parameter));
    }

    bool MaterialParameterBindings::has(std::string_view name) const
    {
        const std::string normalized_name = normalize_uniform_name(name);
        return try_get_uniform_by_name(values, normalized_name) != nullptr;
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
        values.clear();
    }

    void MaterialTextureBindings::set(std::string_view name, Handle texture)
    {
        set(name, TextureInstance(std::move(texture)));
    }

    void MaterialTextureBindings::set(std::string_view name, TextureInstance texture)
    {
        const std::string normalized_name = normalize_uniform_name(name);
        auto* entry = try_get_texture_by_name(overrides, normalized_name);
        if (entry)
        {
            entry->texture = std::move(texture);
            return;
        }

        overrides.push_back(
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

    bool MaterialTextureBindings::has(std::string_view name) const
    {
        const std::string normalized_name = normalize_uniform_name(name);
        return try_get_texture_by_name(overrides, normalized_name) != nullptr;
    }

    void MaterialTextureBindings::remove(std::string_view name)
    {
        const std::string normalized_name = normalize_uniform_name(name);
        for (auto it = overrides.begin(); it != overrides.end(); ++it)
        {
            if (it->name != normalized_name)
                continue;

            overrides.erase(it);
            return;
        }
    }

    void MaterialTextureBindings::clear()
    {
        overrides.clear();
    }
}
