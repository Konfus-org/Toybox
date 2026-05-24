#pragma once
#include "tbx/systems/debugging/macros.h"
#include "tbx/types/components/material_instance.h"
#include "tbx/types/material.h"
#include <string>
#include <type_traits>
#include <variant>

namespace tbx::internal
{
    static std::optional<std::reference_wrapper<MaterialParameter>> try_get_uniform_by_id(
        std::vector<MaterialParameter>& values,
        const uint32 id)
    {
        for (auto& value : values)
        {
            if (value.id == id)
                return std::ref(value);
        }

        return std::nullopt;
    }

    static std::optional<std::reference_wrapper<const MaterialParameter>> try_get_uniform_by_id(
        const std::vector<MaterialParameter>& values,
        const uint32 id)
    {
        for (const auto& value : values)
        {
            if (value.id == id)
                return std::cref(value);
        }

        return std::nullopt;
    }

    static std::optional<std::reference_wrapper<MaterialTextureBinding>> try_get_texture_by_id(
        std::vector<MaterialTextureBinding>& values,
        const uint32 id)
    {
        for (auto& texture : values)
        {
            if (texture.id == id)
                return std::ref(texture);
        }

        return std::nullopt;
    }

    static std::optional<std::reference_wrapper<const MaterialTextureBinding>> try_get_texture_by_id(
        const std::vector<MaterialTextureBinding>& values,
        const uint32 id)
    {
        for (const auto& texture : values)
        {
            if (texture.id == id)
                return std::cref(texture);
        }

        return std::nullopt;
    }

}
