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

    static ShaderUniform* try_get_uniform_by_name(
        std::vector<ShaderUniform>& uniforms,
        const std::string& normalized_name)
    {
        for (auto& uniform : uniforms)
        {
            if (uniform.name == normalized_name)
                return &uniform;
        }

        return nullptr;
    }

    static const ShaderUniform* try_get_uniform_by_name(
        const std::vector<ShaderUniform>& uniforms,
        const std::string& normalized_name)
    {
        for (const auto& uniform : uniforms)
        {
            if (uniform.name == normalized_name)
                return &uniform;
        }

        return nullptr;
    }

    void MaterialOverrides::set(std::string_view name, UniformData value)
    {
        const std::string normalized_name = normalize_uniform_name(name);

        ShaderUniform* uniform = try_get_uniform_by_name(_uniforms, normalized_name);
        if (uniform)
        {
            uniform->data = std::move(value);
            return;
        }

        _uniforms.push_back(ShaderUniform {
            .name = normalized_name,
            .data = std::move(value),
        });
    }

    ShaderUniform& MaterialOverrides::get(std::string_view name)
    {
        const std::string normalized_name = normalize_uniform_name(name);
        ShaderUniform* uniform = try_get_uniform_by_name(_uniforms, normalized_name);
        TBX_ASSERT(
            uniform != nullptr,
            "MaterialOverrides: uniform '{}' not found.",
            normalized_name);
        return *uniform;
    }

    const ShaderUniform& MaterialOverrides::get(std::string_view name) const
    {
        const std::string normalized_name = normalize_uniform_name(name);
        const ShaderUniform* uniform = try_get_uniform_by_name(_uniforms, normalized_name);
        TBX_ASSERT(
            uniform != nullptr,
            "MaterialOverrides: uniform '{}' not found.",
            normalized_name);
        return *uniform;
    }

    bool MaterialOverrides::try_get(std::string_view name, UniformData& out_value) const
    {
        const std::string normalized_name = normalize_uniform_name(name);
        const ShaderUniform* uniform = try_get_uniform_by_name(_uniforms, normalized_name);
        if (!uniform)
            return false;

        out_value = uniform->data;
        return true;
    }

    bool MaterialOverrides::has(std::string_view name) const
    {
        const std::string normalized_name = normalize_uniform_name(name);
        return try_get_uniform_by_name(_uniforms, normalized_name) != nullptr;
    }

    void MaterialOverrides::remove(std::string_view name)
    {
        const std::string normalized_name = normalize_uniform_name(name);

        for (auto it = _uniforms.begin(); it != _uniforms.end(); ++it)
        {
            if (it->name != normalized_name)
                continue;

            _uniforms.erase(it);
            return;
        }
    }

    void MaterialOverrides::clear()
    {
        _uniforms.clear();
    }

    std::span<const ShaderUniform> MaterialOverrides::get_uniforms() const
    {
        return std::span<const ShaderUniform>(_uniforms.data(), _uniforms.size());
    }
}
