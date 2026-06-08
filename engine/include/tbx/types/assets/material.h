#pragma once
#include "tbx/tbx_api.h"
#include "tbx/types/assets/asset.h"
#include "tbx/types/assets/shader.h"
#include "tbx/types/color.h"
#include "tbx/types/handle.h"
#include "tbx/types/matrices.h"
#include "tbx/types/typedefs.h"
#include "tbx/types/vectors.h"
#include "tbx/utils/hash.h"
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "tbx/types/assets/material.generated.h"

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

    [[serializable]];
    [[hash($)]];
    using MaterialParameterData =
        std::variant<bool, int, float, double, Vec2, Vec3, Vec4, Color, Mat3, Mat4>;

    /// @brief
    /// Purpose: Identifies which fixed renderer upload target a declared material binding feeds.
    /// @details
    /// Ownership: Value type.
    /// Thread Safety: Safe to copy between threads.
    [[serializable]];
    enum class MaterialBindingTarget : uint8_t
    {
        NONE [[name("none")]] = 0,
        BASE_COLOR [[name("base_color")]] = 1,
        EMISSIVE_COLOR [[name("emissive_color")]] = 2,
        METALLIC [[name("metallic")]] = 3,
        ROUGHNESS [[name("roughness")]] = 4,
        NORMAL_STRENGTH [[name("normal_strength")]] = 5,
        AO [[name("ao")]] = 6,
        ALPHA_CUTOFF [[name("alpha_cutoff")]] = 7,
        ALBEDO_TEXTURE [[name("albedo_texture")]] = 8,
        NORMAL_TEXTURE [[name("normal_texture")]] = 9,
        METALLIC_TEXTURE [[name("metallic_texture")]] = 10,
        ROUGHNESS_TEXTURE [[name("roughness_texture")]] = 11,
        AO_TEXTURE [[name("ao_texture")]] = 12,
        EMISSIVE_TEXTURE [[name("emissive_texture")]] = 13,
        SKY_COLOR [[name("sky_color")]] = 14,
        SKY_BRIGHTNESS [[name("sky_brightness")]] = 15,
        SKY_TEXTURE [[name("sky_texture")]] = 16
    };

    /// @brief
    /// Purpose: Selects the depth comparison function used when rendering a material.
    /// @details
    /// Ownership: Value type.
    /// Thread Safety: Safe to copy between threads.
    [[serializable]];
    enum class MaterialDepthFunction : uint8_t
    {
        LESS [[name("less")]] = 0,
        LESS_EQUAL [[name("less_equal")]] = 1,
        ALWAYS [[name("always")]] = 2
    };

    /// @brief
    /// Purpose: Selects the transparency path used when rendering a material.
    /// @details
    /// Ownership: Value type.
    /// Thread Safety: Safe to copy between threads.
    [[serializable]];
    enum class MaterialBlendMode : uint8_t
    {
        OPAQUE [[name("opaque")]] = 0,
        ALPHA_BLEND [[name("alpha_blend")]] = 1,
        TRANSPARENT [[name("transparent")]] = 2,
    };

    /// @brief
    /// Purpose: Controls how a material participates in realtime shadowing.
    /// @details
    /// Ownership: Value type.
    /// Thread Safety: Safe to copy between threads.
    [[serializable]];
    enum class ShadowMode : uint8_t
    {
        OFF [[name("off")]] = 0,
        ON [[name("on")]] = 1
    };

    /// @brief
    /// Purpose: Stores one material parameter value keyed by a stable hashed id.
    /// @details
    /// Ownership: Stores the parameter payload inline.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    [[serializable]];
    [[hash(id, data, target)]];
    struct TBX_API MaterialParameter
    {
        MaterialParameter() = default;
        MaterialParameter(uint32 parameter_id, MaterialParameterData parameter_data);

        template <typename TValue>
        MaterialParameter(std::string_view parameter_name, TValue&& parameter_data);
        template <typename TValue>
        MaterialParameter(const std::string& parameter_name, TValue&& parameter_data);
        template <typename TValue>
        MaterialParameter(const char* parameter_name, TValue&& parameter_data);

        [[prop]]
        std::string name = "";
        uint32 id = INVALID_MATERIAL_PARAM_ID;

        [[prop]]
        MaterialParameterData data = 0.0f;

        [[prop]]
        MaterialBindingTarget target = MaterialBindingTarget::NONE;
    };

    /// @brief
    /// Purpose: Stores parameter bindings for a material or material instance.
    /// @details
    /// Ownership: Owns all parameter entries by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    [[serializable]];
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

        [[prop]]
        std::vector<MaterialParameter> values = {};
    };

    /// @brief
    /// Purpose: Stores one texture binding keyed by a stable hashed id.
    /// @details
    /// Ownership: Owns the texture instance by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    [[serializable]];
    [[hash(id, texture.id, texture.name, target)]];
    struct TBX_API MaterialTextureBinding
    {
        MaterialTextureBinding() = default;
        MaterialTextureBinding(uint32 binding_id, Handle texture_handle);
        MaterialTextureBinding(const char* binding_name, Handle texture_handle);
        MaterialTextureBinding(const std::string& binding_name, Handle texture_handle);
        MaterialTextureBinding(std::string_view binding_name, Handle texture_handle);

        [[prop]]
        std::string name = "";
        uint32 id = INVALID_MATERIAL_PARAM_ID;

        [[prop]]
        Handle texture = {};

        [[prop]]
        MaterialBindingTarget target = MaterialBindingTarget::NONE;
    };

    /// @brief
    /// Purpose: Stores texture bindings for a material or material instance.
    /// @details
    /// Ownership: Owns all texture entries by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    [[serializable]];
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

        [[prop]]
        std::vector<MaterialTextureBinding> values = {};
    };

    /// @brief
    /// Purpose: Stores render-state configuration for a material.
    /// @details
    /// Ownership: Value type owned by the containing material asset.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    [[serializable]];
    [[hash(
        is_depth_test_enabled,
        is_depth_write_enabled,
        is_depth_prepass_enabled,
        is_two_sided,
        is_cullable,
        depth_function,
        blend_mode,
        shadow_mode)]];
    struct TBX_API MaterialConfig
    {
        [[prop]]
        bool is_depth_test_enabled = true;

        [[prop]]
        bool is_depth_write_enabled = true;

        [[prop]]
        bool is_depth_prepass_enabled = false;

        [[prop]]
        bool is_two_sided = false;

        [[prop]]
        bool is_cullable = true;

        [[prop]]
        MaterialDepthFunction depth_function = MaterialDepthFunction::LESS;

        [[prop]]
        MaterialBlendMode blend_mode = MaterialBlendMode::OPAQUE;

        [[prop]]
        ShadowMode shadow_mode = ShadowMode::ON;
    };

    /// @brief
    /// Purpose: Stores the shader program, default bindings, and render config for a material
    /// asset.
    /// @details
    /// Ownership: Owns shader handles, default parameter bindings, default texture bindings, and
    /// config by value. Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    [[serializable]];
    [[version(1U)]];
    struct TBX_API Material : Asset
    {
        [[prop]]
        ShaderProgram shader = {};

        [[prop]]
        MaterialParameterBindings parameters = {};

        [[prop]]
        MaterialTextureBindings textures = {};

        [[prop]]
        MaterialConfig config = {};
    };

}

#include "tbx/types/assets/material.inl"
