#pragma once
#include "tbx/systems/files/serialization.h"
#include "tbx/tbx_api.h"
#include "tbx/types/color.h"
#include "tbx/types/handle.h"
#include "tbx/types/shader.h"
#include "tbx/types/vectors.h"
#include "tbx/utils/hash.h"
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace tbx
{
    using MaterialParameterData =
        std::variant<bool, int, float, double, Vec2, Vec3, Vec4, Color, Mat3, Mat4>;

    /// @brief
    /// Purpose: Selects the depth comparison function used when rendering a material.
    /// @details
    /// Ownership: Value type.
    /// Thread Safety: Safe to copy between threads.
    enum class MaterialDepthFunction : uint8_t
    {
        LESS = 0,
        LESS_EQUAL = 1,
        ALWAYS = 2
    };

    /// @brief
    /// Purpose: Selects the transparency path used when rendering a material.
    /// @details
    /// Ownership: Value type.
    /// Thread Safety: Safe to copy between threads.
    enum class MaterialBlendMode : uint8_t
    {
        OPAQUE = 0,
        ALPHA_BLEND = 1
    };

    /// @brief
    /// Purpose: Controls when shadows are rendered for a material. Always ignores the global
    /// shadow caster distance limit so distant geometry can still cast.
    /// @details
    /// Ownership: Value type.
    /// Thread Safety: Safe to copy between threads.
    enum class ShadowMode : uint8_t
    {
        NONE = 0,
        STANDARD = 1,
        ALWAYS = 2
    };

    /// @brief
    /// Purpose: Stores one named material parameter value.
    /// @details
    /// Ownership: Owns the parameter name by value and stores the parameter payload inline.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API MaterialParameter
    {
        MaterialParameter() = default;

        template <typename TValue>
        MaterialParameter(std::string_view parameter_name, TValue&& parameter_data);

        std::string name = "";
        MaterialParameterData data = 0.0f;
    };

    /// @brief
    /// Purpose: Stores named parameter bindings for a material or material instance.
    /// @details
    /// Ownership: Owns all parameter entries by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API MaterialParameterBindings
    {
        using iterator = std::vector<MaterialParameter>::iterator;
        using const_iterator = std::vector<MaterialParameter>::const_iterator;

        MaterialParameterBindings() = default;
        MaterialParameterBindings(std::initializer_list<MaterialParameter> parameters)
            : values(parameters)
        {
        }

        void set(std::string_view name, MaterialParameterData value);
        void set(MaterialParameter parameter);
        void set(std::initializer_list<MaterialParameter> parameters);
        std::optional<std::reference_wrapper<MaterialParameter>> get(std::string_view name);
        std::optional<std::reference_wrapper<const MaterialParameter>> get(
            std::string_view name) const;
        bool has(std::string_view name) const;
        void remove(std::string_view name);
        void clear();

        iterator begin();
        const_iterator begin() const;
        const_iterator cbegin() const;
        iterator end();
        const_iterator end() const;
        const_iterator cend() const;

        std::vector<MaterialParameter> values = {};
    };

    /// @brief
    /// Purpose: Stores one named texture binding.
    /// @details
    /// Ownership: Owns the binding name by value and texture instance by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API MaterialTextureBinding
    {
        std::string name = {};
        Handle texture = {};
    };

    /// @brief
    /// Purpose: Stores named texture bindings for a material or material instance.
    /// @details
    /// Ownership: Owns all texture entries by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API MaterialTextureBindings
    {
        using iterator = std::vector<MaterialTextureBinding>::iterator;
        using const_iterator = std::vector<MaterialTextureBinding>::const_iterator;

        MaterialTextureBindings() = default;
        MaterialTextureBindings(std::initializer_list<MaterialTextureBinding> texture_bindings)
            : values(texture_bindings)
        {
        }

        void set(std::string_view name, Handle texture);
        void set(MaterialTextureBinding texture_binding);
        void set(std::initializer_list<MaterialTextureBinding> texture_bindings);
        std::optional<std::reference_wrapper<MaterialTextureBinding>> get(std::string_view name);
        std::optional<std::reference_wrapper<const MaterialTextureBinding>> get(
            std::string_view name) const;
        bool has(std::string_view name) const;
        void remove(std::string_view name);
        void clear();

        iterator begin();
        const_iterator begin() const;
        const_iterator cbegin() const;
        iterator end();
        const_iterator end() const;
        const_iterator cend() const;

        std::vector<MaterialTextureBinding> values = {};
    };

    /// @brief
    /// Purpose: Stores render-state configuration for a material.
    /// @details
    /// Ownership: Value type owned by the containing material asset.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API MaterialConfig
    {
        bool is_depth_test_enabled = true;
        bool is_depth_write_enabled = true;
        bool is_depth_prepass_enabled = false;
        bool is_two_sided = false;
        bool is_cullable = true;

        MaterialDepthFunction depth_function = MaterialDepthFunction::LESS;
        MaterialBlendMode blend_mode = MaterialBlendMode::OPAQUE;
        ShadowMode shadow_mode = ShadowMode::STANDARD;
    };

    /// @brief
    /// Purpose: Stores the shader program, default bindings, and render config for a material
    /// asset.
    /// @details
    /// Ownership: Owns shader handles, default parameter bindings, default texture bindings, and
    /// config by value. Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API Material
    {
        Shader shader = {};
        MaterialParameterBindings parameters = {};
        MaterialTextureBindings textures = {};
        MaterialConfig config = {};
    };

    TBX_API uint64 hash(
        const MaterialParameterData& data,
        uint64 value = TBX_FNV1A_OFFSET_BASIS);
    TBX_API uint64 hash(const MaterialConfig& config, uint64 value = TBX_FNV1A_OFFSET_BASIS);
}

#include "tbx/types/components/material_instance.h"
#include "tbx/types/material.inl"
