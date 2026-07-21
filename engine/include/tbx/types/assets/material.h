#pragma once
// clang-format off
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

// The generated header must come AFTER the type and alias declarations above
// (it references Vec2/MaterialParameterData). Keep it last; clang-format guard
// prevents the include sorter from hoisting it.
#include "tbx/types/assets/material.generated.h"
// clang-format on

namespace tbx
{
    [[serializable]];
    [[hash($)]];
    using MaterialParameterData =
        std::variant<bool, int, float, double, Vec2, Vec3, Vec4, Color, Mat3, Mat4>;

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
    /// Purpose: Classifies a material by the render role it plays, so systems (and the editor) can
    /// reason about a material without inspecting its shader/config. A sky material, for instance,
    /// is previewed as the environment background rather than on a mesh.
    /// @details
    /// Ownership: Value type. Thread Safety: Safe to copy between threads.
    [[serializable]];
    enum class MaterialType : uint8_t
    {
        RASTER [[name("raster")]] = 0, // A standard rasterized surface drawn on mesh geometry.
        SKY [[name("sky")]] = 1, // An environment/background material (skybox or sky-sphere).
        POST [[name("post")]] = 2, // A full-screen post-process effect.
        GEO [[name("geo")]] = 3, // A geometry/depth pass (e.g. shadow or depth pre-pass).
        COMPUTE [[name("compute")]] = 4, // A compute-shader material (no rasterized surface).
    };

    /// @brief
    /// Purpose: Stores one material parameter value keyed by shader binding name.
    /// @details
    /// Ownership: Stores the parameter payload inline.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    [[serializable]];
    [[hash(name, data)]];
    struct TBX_API MaterialParameter
    {
        MaterialParameter() = default;
        MaterialParameter(std::string parameter_name, MaterialParameterData parameter_data);

        template <typename TValue>
        MaterialParameter(std::string_view parameter_name, TValue&& parameter_data);
        template <typename TValue>
        MaterialParameter(const std::string& parameter_name, TValue&& parameter_data);
        template <typename TValue>
        MaterialParameter(const char* parameter_name, TValue&& parameter_data);

        std::string name = "";

        MaterialParameterData data = 0.0f;
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
        void set(MaterialParameter parameter);
        void set(std::initializer_list<MaterialParameter> parameters);
        std::optional<std::reference_wrapper<MaterialParameter>> get(std::string_view name);
        std::optional<std::reference_wrapper<const MaterialParameter>> get(
            std::string_view name) const;
        template <typename TValue>
        TValue get_or(std::string_view name, const TValue& fallback) const;
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
    /// Purpose: Stores one texture binding keyed by shader binding name.
    /// @details
    /// Ownership: Owns the texture instance by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    [[serializable]];
    [[hash(name, texture.id, texture.name)]];
    struct TBX_API MaterialTextureBinding
    {
        MaterialTextureBinding() = default;
        MaterialTextureBinding(std::string binding_name, Handle texture_handle);
        MaterialTextureBinding(const char* binding_name, Handle texture_handle);
        MaterialTextureBinding(std::string_view binding_name, Handle texture_handle);

        std::string name = "";

        Handle texture = {};
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
        bool is_depth_test_enabled = true;

        bool is_depth_write_enabled = true;

        bool is_depth_prepass_enabled = false;

        bool is_two_sided = false;

        bool is_cullable = true;

        MaterialDepthFunction depth_function = MaterialDepthFunction::LESS;

        MaterialBlendMode blend_mode = MaterialBlendMode::OPAQUE;

        ShadowMode shadow_mode = ShadowMode::ON;
    };

    /// @brief The render state (depth/blend/cull) a raster pipeline is built with. Derived from a
    /// MaterialConfig at draw time; together with the shader program it identifies a pipeline (see
    /// the hash overload below), which is what the GpuResourceCache keys compiled pipelines by.
    /// @brief Selects how a blending pipeline combines its output with the existing target. ALPHA
    /// is a colored composite (final = src*src.a + dst*src.rgb): the surface adds its
    /// alpha-weighted color AND tints whatever is behind it by its color. MULTIPLY is a pure
    /// colored filter (dst *= src), also used to accumulate transmittance into the colored
    /// (translucent) shadow map.
    enum class BlendEquation : uint8_t
    {
        ALPHA = 0,
        MULTIPLY = 1,
    };

    struct RasterState
    {
        bool is_blending_enabled = false;
        bool is_two_sided = false;
        bool is_depth_test_enabled = true;
        bool is_depth_write_enabled = true;
        MaterialDepthFunction depth_function = MaterialDepthFunction::LESS;
        BlendEquation blend_equation = BlendEquation::ALPHA;
    };

    /// @brief Folds a pipeline's shader stages + render state into the stable hash it is keyed by.
    inline uint64 hash(const ShaderProgram& shader, const RasterState& state)
    {
        uint64 value = hash(shader);
        value = hash_combine(value, static_cast<uint64>(state.is_blending_enabled));
        value = hash_combine(value, static_cast<uint64>(state.is_two_sided));
        value = hash_combine(value, static_cast<uint64>(state.is_depth_test_enabled));
        value = hash_combine(value, static_cast<uint64>(state.is_depth_write_enabled));
        value = hash_combine(value, static_cast<uint64>(state.depth_function));
        value = hash_combine(value, static_cast<uint64>(state.blend_equation));
        return value;
    }

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
        MaterialType type = MaterialType::RASTER;

        ShaderProgram shader = {};

        MaterialParameterBindings parameters = {};

        MaterialTextureBindings textures = {};

        MaterialConfig config = {};
    };
}

#include "tbx/types/assets/material.inl"
