#pragma once
#include "tbx/core/types/handle.h"
#include "tbx/core/systems/graphics/color.h"
#include "tbx/core/systems/graphics/shader.h"
#include "tbx/core/systems/math/vectors.h"
#include "tbx/core/tbx_api.h"
#include <cstdint>
#include <initializer_list>
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
        Less = 0,
        LessEqual = 1,
        Always = 2
    };

    /// @brief
    /// Purpose: Selects the transparency path used when rendering a material.
    /// @details
    /// Ownership: Value type.
    /// Thread Safety: Safe to copy between threads.
    enum class MaterialBlendMode : uint8_t
    {
        Opaque = 0,
        AlphaBlend = 1
    };

    /// @brief
    /// Purpose: Controls when shadows are rendered for a material.
    /// @details
    /// Ownership: Value type.
    /// Thread Safety: Safe to copy between threads.
    enum class ShadowMode : uint8_t
    {
        None = 0,
        Standard = 1,
        Always = 2
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
        MaterialParameter* get(std::string_view name);
        const MaterialParameter* get(std::string_view name) const;
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
        MaterialTextureBinding* get(std::string_view name);
        const MaterialTextureBinding* get(std::string_view name) const;
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
        MaterialDepthFunction depth_function = MaterialDepthFunction::Less;
        MaterialBlendMode blend_mode = MaterialBlendMode::Opaque;
        bool is_two_sided = false;
        bool is_cullable = true;
        ShadowMode shadow_mode = ShadowMode::Standard;
    };

    /// @brief
    /// Purpose: Stores the shader program, default bindings, and render config for a material
    /// asset.
    /// @details
    /// Ownership: Owns shader handles, default parameter bindings, default texture bindings, and
    /// config by value. Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API Material
    {
        ShaderProgram program = {};
        MaterialParameterBindings parameters = {};
        MaterialTextureBindings textures = {};
        MaterialConfig config = {};
    };

    /// @brief
    /// Purpose: Stores a material asset handle plus flat runtime override data.
    /// @details
    /// Ownership: Owns the material handle and all override bindings by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API MaterialInstance
    {
        MaterialInstance();
        MaterialInstance(Handle handle);
        MaterialInstance(
            Handle handle,
            MaterialParameterBindings parameter_overrides,
            MaterialTextureBindings texture_overrides = {},
            MaterialConfig config_override = {},
            bool has_config_override = false);

        bool is_dirty() const;
        void clear_dirty();
        void mark_dirty();

        const Handle& get_handle() const;
        bool has_config_override_enabled() const;
        void set_config(MaterialConfig config_override);
        void set_parameter(std::string_view name, MaterialParameterData value);
        void set_texture(std::string_view name, Handle texture);

        template <typename TValue>
        TValue get_parameter_or(std::string_view name, const TValue& fallback) const;

        bool get_bool_parameter_or(std::string_view name, bool fallback) const;
        int get_int_parameter_or(std::string_view name, int fallback) const;
        float get_float_parameter_or(std::string_view name, float fallback) const;
        double get_double_parameter_or(std::string_view name, double fallback) const;
        Handle get_texture_handle_or(std::string_view name, const Handle& fallback = {}) const;

        Handle material = {};
        MaterialTextureBindings texture_overrides = {};
        MaterialParameterBindings param_overrides = {};
        MaterialConfig config = {};

      private:
        bool _is_dirty = true;
        bool _has_config_override = false;
    };

    /// @brief
    /// Purpose: Stores the sky material instance used for environment rendering.
    /// @details
    /// Ownership: Owns the material instance by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API Sky
    {
        MaterialInstance material = {};
    };
}

#include "tbx/core/systems/graphics/material.inl"
