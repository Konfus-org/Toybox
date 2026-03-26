#pragma once
#include "tbx/common/handle.h"
#include "tbx/graphics/color.h"
#include "tbx/graphics/shader.h"
#include "tbx/graphics/texture.h"
#include "tbx/math/vectors.h"
#include "tbx/tbx_api.h"
#include <initializer_list>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace tbx
{
    using MaterialParameterData =
        std::variant<bool, int, float, double, Vec2, Vec3, Vec4, Color, Mat3, Mat4>;

    using UniformData = MaterialParameterData;

    struct TBX_API MaterialParameter
    {
        std::string name = "";
        MaterialParameterData data = 0.0f;
    };

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

    struct TBX_API MaterialTextureBinding
    {
        std::string name = {};
        TextureInstance texture = {};
    };

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
        void set(std::string_view name, TextureInstance texture);
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

    struct TBX_API Material
    {
        ShaderProgram program = {};
        MaterialTextureBindings textures = {};
        MaterialParameterBindings parameters = {};
    };

    struct TBX_API MaterialInstance
    {
        Handle handle = {};
        MaterialParameterBindings parameters = {};
        MaterialTextureBindings textures = {};
        bool has_loaded_defaults = false;
    };

    /// <summary>
    /// Purpose: PBR-oriented material instance with fixed default texture and parameter properties.
    /// </summary>
    /// <remarks>
    /// Ownership: Inherits and owns MaterialInstance binding state by value.
    /// Thread Safety: Safe for concurrent reads; synchronize external mutation.
    /// </remarks>
    struct TBX_API PbrMaterialInstance : public MaterialInstance
    {
        PbrMaterialInstance();
        PbrMaterialInstance(
            Color color,
            Color emissive_color = Color(0.0f, 0.0f, 0.0f, 1.0f),
            float specular_strength = 0.5f,
            float shininess_strength = 32.0f,
            float alpha_cutoff = 0.1f,
            float diffuse_strength = 1.0f,
            float normal_strength = 1.0f,
            float emissive_strength = 1.0f,
            const Handle& diffuse_map = Handle(),
            const Handle& normal_map = Handle(),
            const Handle& specular_map = Handle(),
            const Handle& shininess_map = Handle(),
            const Handle& emissive_map = Handle(),
            const Handle& material_handle = Handle(),
            float transparency_amount = 0.0f);
        PbrMaterialInstance(const MaterialInstance& other);

        void set_diffuse_map(Handle value);
        Handle get_diffuse_map() const;

        void set_normal_map(Handle value);
        Handle get_normal_map() const;

        void set_specular_map(Handle value);
        Handle get_specular_map() const;

        void set_shininess_map(Handle value);
        Handle get_shininess_map() const;

        void set_emissive_map(Handle value);
        Handle get_emissive_map() const;

        void set_color(Color value);
        Color get_color() const;

        void set_diffuse_strength(float value);
        float get_diffuse_strength() const;

        void set_normal_strength(float value);
        float get_normal_strength() const;

        void set_specular_strength(float value);
        float get_specular_strength() const;

        void set_shininess_strength(float value);
        float get_shininess_strength() const;

        void set_emissive_color(Color value);
        Color get_emissive_color() const;

        void set_emissive_strength(float value);
        float get_emissive_strength() const;

        void set_alpha_cutoff(float value);
        float get_alpha_cutoff() const;

        void set_transparency_amount(float value);
        float get_transparency_amount() const;

        void set_exposure(float value);
        float get_exposure() const;
    };

    /// <summary>
    /// Purpose: Unlit material instance with fixed default texture and parameter properties.
    /// </summary>
    /// <remarks>
    /// Ownership: Inherits and owns MaterialInstance binding state by value.
    /// Thread Safety: Safe for concurrent reads; synchronize external mutation.
    /// </remarks>
    struct TBX_API FlatMaterialInstance : public MaterialInstance
    {
        FlatMaterialInstance();
        FlatMaterialInstance(
            Color color,
            Color emissive_color = Color(0.0f, 0.0f, 0.0f, 1.0f),
            float alpha_cutoff = 0.1f,
            const Handle& diffuse_map = Handle(),
            const Handle& material_handle = Handle(),
            float transparency_amount = 0.0f,
            float exposure = 1.0f);
        FlatMaterialInstance(const MaterialInstance& other);

        void set_diffuse_map(Handle value);
        Handle get_diffuse_map() const;

        void set_color(Color value);
        Color get_color() const;

        void set_emissive_color(Color value);
        Color get_emissive_color() const;

        void set_alpha_cutoff(float value);
        float get_alpha_cutoff() const;

        void set_transparency_amount(float value);
        float get_transparency_amount() const;

        void set_exposure(float value);
        float get_exposure() const;
    };

    struct TBX_API Sky
    {
        MaterialInstance material = {};
    };
}
