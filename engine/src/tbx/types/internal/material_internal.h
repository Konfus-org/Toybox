#pragma once
#include "tbx/systems/debugging/macros.h"
#include "tbx/types/components/material_instance.h"
#include "tbx/types/material.h"
#include <string>
#include <type_traits>
#include <variant>

namespace tbx::internal
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

}
