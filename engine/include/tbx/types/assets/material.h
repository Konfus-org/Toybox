#pragma once
#include "tbx/systems/assets/serialization.h"
#include "tbx/tbx_api.h"
#include "tbx/types/assets/asset.h"
#include "tbx/types/assets/shader.h"
#include "tbx/types/color.h"
#include "tbx/types/handle.h"
#include "tbx/types/typedefs.h"
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
    inline constexpr uint32 INVALID_MATERIAL_PARAM_ID = 0U;

    constexpr uint32 make_param_id(const std::string_view name)
    {
        auto result = TBX_FNV1A_OFFSET_BASIS;
        for (const char value : name)
        {
            result ^= static_cast<unsigned char>(value);
            result *= TBX_FNV1A_PRIME;
        }
        return static_cast<uint32>(result);
    }

    using MaterialParameterData =
        std::variant<bool, int, float, double, Vec2, Vec3, Vec4, Color, Mat3, Mat4>;

    TBX_REGISTER_SERIALIZABLE_VARIANT(MaterialParameterData)

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

    TBX_REGISTER_SERIALIZABLE_ENUM(
        MaterialDepthFunction,
        {
            {MaterialDepthFunction::LESS, "less"},
            {MaterialDepthFunction::LESS_EQUAL, "less_equal"},
            {MaterialDepthFunction::ALWAYS, "always"},
        })

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

    TBX_REGISTER_SERIALIZABLE_ENUM(
        MaterialBlendMode,
        {
            {MaterialBlendMode::OPAQUE, "opaque"},
            {MaterialBlendMode::ALPHA_BLEND, "alpha_blend"},
        })

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

    TBX_REGISTER_SERIALIZABLE_ENUM(
        ShadowMode,
        {
            {ShadowMode::NONE, "none"},
            {ShadowMode::STANDARD, "standard"},
            {ShadowMode::ALWAYS, "always"},
        })

    /// @brief
    /// Purpose: Stores one material parameter value keyed by a stable hashed id.
    /// @details
    /// Ownership: Stores the parameter payload inline.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API MaterialParameter
    {
        MaterialParameter() = default;
        MaterialParameter(uint32 parameter_id, MaterialParameterData parameter_data);

        template <typename TValue>
        MaterialParameter(std::string_view parameter_name, TValue&& parameter_data);
        template <typename TValue>
        MaterialParameter(const std::string& parameter_name, TValue&& parameter_data);

        std::string name = "";
        uint32 id = INVALID_MATERIAL_PARAM_ID;
        MaterialParameterData data = 0.0f;
    };

    TBX_REGISTER_SERIALIZABLE_STRUCT(MaterialParameter, name, data)

    /// @brief
    /// Purpose: Stores parameter bindings for a material or material instance.
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
        void set(uint32 id, MaterialParameterData value);
        void set(MaterialParameter parameter);
        void set(std::initializer_list<MaterialParameter> parameters);
        std::optional<std::reference_wrapper<MaterialParameter>> get(std::string_view name);
        std::optional<std::reference_wrapper<const MaterialParameter>> get(
            std::string_view name) const;
        std::optional<std::reference_wrapper<MaterialParameter>> get(uint32 id);
        std::optional<std::reference_wrapper<const MaterialParameter>> get(uint32 id) const;
        bool has(std::string_view name) const;
        bool has(uint32 id) const;
        void remove(std::string_view name);
        void remove(uint32 id);
        void clear();

        iterator begin();
        const_iterator begin() const;
        const_iterator cbegin() const;
        iterator end();
        const_iterator end() const;
        const_iterator cend() const;

        std::vector<MaterialParameter> values = {};
    };

    TBX_REGISTER_SERIALIZABLE_STRUCT(MaterialParameterBindings, values)

    /// @brief
    /// Purpose: Stores one texture binding keyed by a stable hashed id.
    /// @details
    /// Ownership: Owns the texture instance by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API MaterialTextureBinding
    {
        MaterialTextureBinding() = default;
        MaterialTextureBinding(uint32 binding_id, Handle texture_handle);
        MaterialTextureBinding(const std::string& binding_name, Handle texture_handle);
        MaterialTextureBinding(std::string_view binding_name, Handle texture_handle);

        std::string name = "";
        uint32 id = INVALID_MATERIAL_PARAM_ID;
        Handle texture = {};
    };

    TBX_REGISTER_SERIALIZABLE_STRUCT(MaterialTextureBinding, name, texture)

    /// @brief
    /// Purpose: Stores texture bindings for a material or material instance.
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
        void set(uint32 id, Handle texture);
        void set(MaterialTextureBinding texture_binding);
        void set(std::initializer_list<MaterialTextureBinding> texture_bindings);
        std::optional<std::reference_wrapper<MaterialTextureBinding>> get(std::string_view name);
        std::optional<std::reference_wrapper<const MaterialTextureBinding>> get(
            std::string_view name) const;
        std::optional<std::reference_wrapper<MaterialTextureBinding>> get(uint32 id);
        std::optional<std::reference_wrapper<const MaterialTextureBinding>> get(uint32 id) const;
        bool has(std::string_view name) const;
        bool has(uint32 id) const;
        void remove(std::string_view name);
        void remove(uint32 id);
        void clear();

        iterator begin();
        const_iterator begin() const;
        const_iterator cbegin() const;
        iterator end();
        const_iterator end() const;
        const_iterator cend() const;

        std::vector<MaterialTextureBinding> values = {};
    };

    TBX_REGISTER_SERIALIZABLE_STRUCT(MaterialTextureBindings, values)

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

    TBX_REGISTER_SERIALIZABLE_STRUCT(
        MaterialConfig,
        is_depth_test_enabled,
        is_depth_write_enabled,
        is_depth_prepass_enabled,
        is_two_sided,
        is_cullable,
        depth_function,
        blend_mode,
        shadow_mode)

    /// @brief
    /// Purpose: Stores the shader program, default bindings, and render config for a material
    /// asset.
    /// @details
    /// Ownership: Owns shader handles, default parameter bindings, default texture bindings, and
    /// config by value. Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API Material : Asset
    {
        Shader shader = {};
        MaterialParameterBindings parameters = {};
        MaterialTextureBindings textures = {};
        MaterialConfig config = {};
    };

    TBX_REGISTER_SERIALIZABLE_ASSET(Material, 1U, shader, parameters, textures, config)

    TBX_API uint64 hash(const MaterialParameterData& data, uint64 value = TBX_FNV1A_OFFSET_BASIS);
    TBX_API uint64 hash(const MaterialConfig& config, uint64 value = TBX_FNV1A_OFFSET_BASIS);
}

template <>
struct std::hash<tbx::MaterialParameterData>
{
    ::size operator()(const tbx::MaterialParameterData& value) const
    {
        return static_cast<::size>(tbx::hash(value));
    }
};

template <>
struct std::hash<tbx::MaterialConfig>
{
    ::size operator()(const tbx::MaterialConfig& value) const
    {
        return static_cast<::size>(tbx::hash(value));
    }
};

#include "tbx/types/assets/material.inl"
#include "tbx/types/components/material_instance.h"
