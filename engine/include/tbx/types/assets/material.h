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

    [[tbx::serializable]];
    [[tbx::hash(tbx::hash($))]];
    using MaterialParameterData =
        std::variant<bool, int, float, double, Vec2, Vec3, Vec4, Color, Mat3, Mat4>;

    /// @brief
    /// Purpose: Selects the depth comparison function used when rendering a material.
    /// @details
    /// Ownership: Value type.
    /// Thread Safety: Safe to copy between threads.
    [[tbx::serializable]];
    enum class MaterialDepthFunction : uint8_t
    {
        LESS [[tbx::name("less")]] = 0,
        LESS_EQUAL [[tbx::name("less_equal")]] = 1,
        ALWAYS [[tbx::name("always")]] = 2
    };

    /// @brief
    /// Purpose: Selects the transparency path used when rendering a material.
    /// @details
    /// Ownership: Value type.
    /// Thread Safety: Safe to copy between threads.
    [[tbx::serializable]];
    enum class MaterialBlendMode : uint8_t
    {
        OPAQUE [[tbx::name("opaque")]] = 0,
        ALPHA_BLEND [[tbx::name("alpha_blend")]] = 1
    };

    /// @brief
    /// Purpose: Controls when shadows are rendered for a material. Always ignores the global
    /// shadow caster distance limit so distant geometry can still cast.
    /// @details
    /// Ownership: Value type.
    /// Thread Safety: Safe to copy between threads.
    [[tbx::serializable]];
    enum class ShadowMode : uint8_t
    {
        NONE [[tbx::name("none")]] = 0,
        STANDARD [[tbx::name("standard")]] = 1,
        ALWAYS [[tbx::name("always")]] = 2
    };

    /// @brief
    /// Purpose: Stores one material parameter value keyed by a stable hashed id.
    /// @details
    /// Ownership: Stores the parameter payload inline.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    [[tbx::serializable]];
    struct TBX_API MaterialParameter
    {
        MaterialParameter() = default;
        MaterialParameter(uint32 parameter_id, MaterialParameterData parameter_data);

        template <typename TValue>
        MaterialParameter(std::string_view parameter_name, TValue&& parameter_data);
        template <typename TValue>
        MaterialParameter(const std::string& parameter_name, TValue&& parameter_data);

        [[tbx::prop]]
        std::string name = "";
        uint32 id = INVALID_MATERIAL_PARAM_ID;

        [[tbx::prop]]
        MaterialParameterData data = 0.0f;
    };

    /// @brief
    /// Purpose: Stores parameter bindings for a material or material instance.
    /// @details
    /// Ownership: Owns all parameter entries by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    [[tbx::serializable]];
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

        [[tbx::prop]]
        std::vector<MaterialParameter> values = {};
    };

    /// @brief
    /// Purpose: Stores one texture binding keyed by a stable hashed id.
    /// @details
    /// Ownership: Owns the texture instance by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    [[tbx::serializable]];
    struct TBX_API MaterialTextureBinding
    {
        MaterialTextureBinding() = default;
        MaterialTextureBinding(uint32 binding_id, Handle texture_handle);
        MaterialTextureBinding(const std::string& binding_name, Handle texture_handle);
        MaterialTextureBinding(std::string_view binding_name, Handle texture_handle);

        [[tbx::prop]]
        std::string name = "";
        uint32 id = INVALID_MATERIAL_PARAM_ID;

        [[tbx::prop]]
        Handle texture = {};
    };

    /// @brief
    /// Purpose: Stores texture bindings for a material or material instance.
    /// @details
    /// Ownership: Owns all texture entries by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    [[tbx::serializable]];
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

        [[tbx::prop]]
        std::vector<MaterialTextureBinding> values = {};
    };

    /// @brief
    /// Purpose: Stores render-state configuration for a material.
    /// @details
    /// Ownership: Value type owned by the containing material asset.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    [[tbx::serializable]];
    [[tbx::hash(tbx::hash($))]];
    [[tbx::prop(
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
    [[tbx::serializable]];
    [[tbx::version(1U)]];
    struct TBX_API Material : Asset
    {
        [[tbx::prop]]
        ShaderProgram shader = {};

        [[tbx::prop]]
        MaterialParameterBindings parameters = {};

        [[tbx::prop]]
        MaterialTextureBindings textures = {};

        [[tbx::prop]]
        MaterialConfig config = {};
    };

    TBX_API uint64 hash(const MaterialParameterData& data, uint64 value = TBX_FNV1A_OFFSET_BASIS);
    TBX_API uint64 hash(const MaterialConfig& config, uint64 value = TBX_FNV1A_OFFSET_BASIS);
}

#include "tbx/types/assets/material.generated.h"
#include "tbx/types/assets/material.inl"
