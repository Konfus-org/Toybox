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
        MaterialParameterData data = 0;
    };

    struct TBX_API MaterialParameterBinding
    {
        std::string name = {};
        MaterialParameterData value = 0.0f;
    };

    struct TBX_API MaterialParameterBindings
    {
        using iterator = std::vector<MaterialParameterBinding>::iterator;
        using const_iterator = std::vector<MaterialParameterBinding>::const_iterator;

        MaterialParameterBindings() = default;
        MaterialParameterBindings(std::initializer_list<MaterialParameterBinding> parameters)
            : values(parameters)
        {
        }

        void set(std::string_view name, MaterialParameterData value);
        void set(MaterialParameterBinding parameter);
        void set(std::initializer_list<MaterialParameterBinding> parameters);
        MaterialParameterBinding* get(std::string_view name);
        const MaterialParameterBinding* get(std::string_view name) const;
        bool has(std::string_view name) const;
        void remove(std::string_view name);
        void clear();
        iterator begin();
        const_iterator begin() const;
        const_iterator cbegin() const;
        iterator end();
        const_iterator end() const;
        const_iterator cend() const;

        std::vector<MaterialParameterBinding> values = {};
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
    /// Purpose: Structured material instance with fixed default texture and parameter properties.
    /// </summary>
    /// <remarks>
    /// Ownership: Inherits and owns MaterialInstance binding state by value.
    /// Thread Safety: Safe for concurrent reads; synchronize external mutation.
    /// </remarks>
    struct TBX_API StandardMaterialInstance : public MaterialInstance
    {
        StandardMaterialInstance();
        StandardMaterialInstance(
            Color color,
            Color emissive = Color(0.0f, 0.0f, 0.0f, 1.0f),
            float specular = 0.5f,
            float shininess = 32.0f,
            float occlusion = 1.0f,
            float alpha_cutoff = 0.1f,
            float diffuse_map_strength = 1.0f,
            float normal_map_strength = 1.0f,
            float specular_map_strength = 1.0f,
            float shininess_map_strength = 1.0f,
            float emissive_map_strength = 1.0f,
            float occlusion_map_strength = 1.0f,
            const Handle& diffuse_map = Handle(),
            const Handle& normal_map = Handle(),
            const Handle& specular_map = Handle(),
            const Handle& shininess_map = Handle(),
            const Handle& emissive_map = Handle(),
            const Handle& occlusion_map = Handle(),
            const Handle& material_handle = Handle(),
            float transparency_amount = 0.0f);
        StandardMaterialInstance(const MaterialInstance& other);

        /// <summary>
        /// Purpose: Sets the diffuse map handle used by the material.
        /// Ownership: Stores a non-owning asset handle by value.
        /// Thread Safety: Safe to call with external synchronization.
        /// </summary>
        void set_diffuse_map(Handle value);
        /// <summary>
        /// Purpose: Returns the diffuse map handle used by the material.
        /// Ownership: Returns a value copy; the caller owns the result.
        /// Thread Safety: Safe for concurrent reads.
        /// </summary>
        Handle get_diffuse_map() const;
        /// <summary>
        /// Purpose: Sets the tangent-space normal map handle used by the material.
        /// Ownership: Stores a non-owning asset handle by value.
        /// Thread Safety: Safe to call with external synchronization.
        /// </summary>
        void set_normal_map(Handle value);
        /// <summary>
        /// Purpose: Returns the tangent-space normal map handle used by the material.
        /// Ownership: Returns a value copy; the caller owns the result.
        /// Thread Safety: Safe for concurrent reads.
        /// </summary>
        Handle get_normal_map() const;
        /// <summary>
        /// Purpose: Sets the specular intensity map handle used by the material.
        /// Ownership: Stores a non-owning asset handle by value.
        /// Thread Safety: Safe to call with external synchronization.
        /// </summary>
        void set_specular_map(Handle value);
        /// <summary>
        /// Purpose: Returns the specular intensity map handle used by the material.
        /// Ownership: Returns a value copy; the caller owns the result.
        /// Thread Safety: Safe for concurrent reads.
        /// </summary>
        Handle get_specular_map() const;
        /// <summary>
        /// Purpose: Sets the shininess map handle used by the material.
        /// Ownership: Stores a non-owning asset handle by value.
        /// Thread Safety: Safe to call with external synchronization.
        /// </summary>
        void set_shininess_map(Handle value);
        /// <summary>
        /// Purpose: Returns the shininess map handle used by the material.
        /// Ownership: Returns a value copy; the caller owns the result.
        /// Thread Safety: Safe for concurrent reads.
        /// </summary>
        Handle get_shininess_map() const;
        /// <summary>
        /// Purpose: Sets the emissive map handle used by the material.
        /// Ownership: Stores a non-owning asset handle by value.
        /// Thread Safety: Safe to call with external synchronization.
        /// </summary>
        void set_emissive_map(Handle value);
        /// <summary>
        /// Purpose: Returns the emissive map handle used by the material.
        /// Ownership: Returns a value copy; the caller owns the result.
        /// Thread Safety: Safe for concurrent reads.
        /// </summary>
        Handle get_emissive_map() const;
        /// <summary>
        /// Purpose: Sets the occlusion map handle used by the material.
        /// Ownership: Stores a non-owning asset handle by value.
        /// Thread Safety: Safe to call with external synchronization.
        /// </summary>
        void set_occlusion_map(Handle value);
        /// <summary>
        /// Purpose: Returns the occlusion map handle used by the material.
        /// Ownership: Returns a value copy; the caller owns the result.
        /// Thread Safety: Safe for concurrent reads.
        /// </summary>
        Handle get_occlusion_map() const;
        /// <summary>
        /// Purpose: Sets the albedo tint used by the material.
        /// Ownership: Stores the color by value in the material parameter bindings.
        /// Thread Safety: Safe to call with external synchronization.
        /// </summary>
        void set_color(Color value);
        /// <summary>
        /// Purpose: Returns the albedo tint used by the material.
        /// Ownership: Returns a value copy; the caller owns the result.
        /// Thread Safety: Safe for concurrent reads.
        /// </summary>
        Color get_color() const;
        /// <summary>
        /// Purpose: Sets the strength used when blending the diffuse map with the base color.
        /// Ownership: Stores the value in the material parameter bindings.
        /// Thread Safety: Safe to call with external synchronization.
        /// </summary>
        void set_diffuse_map_strength(float value);
        /// <summary>
        /// Purpose: Returns the strength used when blending the diffuse map with the base color.
        /// Ownership: Returns a value copy; the caller owns the result.
        /// Thread Safety: Safe for concurrent reads.
        /// </summary>
        float get_diffuse_map_strength() const;
        /// <summary>
        /// Purpose: Sets the strength applied to the tangent-space normal map perturbation.
        /// Ownership: Stores the value in the material parameter bindings.
        /// Thread Safety: Safe to call with external synchronization.
        /// </summary>
        void set_normal_map_strength(float value);
        /// <summary>
        /// Purpose: Returns the strength applied to the tangent-space normal map perturbation.
        /// Ownership: Returns a value copy; the caller owns the result.
        /// Thread Safety: Safe for concurrent reads.
        /// </summary>
        float get_normal_map_strength() const;
        /// <summary>
        /// Purpose: Sets the scalar Blinn-Phong specular intensity used by the material.
        /// Ownership: Stores the value in the material parameter bindings.
        /// Thread Safety: Safe to call with external synchronization.
        /// </summary>
        void set_specular(float value);
        /// <summary>
        /// Purpose: Returns the scalar Blinn-Phong specular intensity used by the material.
        /// Ownership: Returns a value copy; the caller owns the result.
        /// Thread Safety: Safe for concurrent reads.
        /// </summary>
        float get_specular() const;
        /// <summary>
        /// Purpose: Sets the strength used when blending the specular map with the base specular.
        /// Ownership: Stores the value in the material parameter bindings.
        /// Thread Safety: Safe to call with external synchronization.
        /// </summary>
        void set_specular_map_strength(float value);
        /// <summary>
        /// Purpose: Returns the strength used when blending the specular map with the base specular.
        /// Ownership: Returns a value copy; the caller owns the result.
        /// Thread Safety: Safe for concurrent reads.
        /// </summary>
        float get_specular_map_strength() const;
        /// <summary>
        /// Purpose: Sets the Blinn-Phong shininess exponent used by the material.
        /// Ownership: Stores the value in the material parameter bindings.
        /// Thread Safety: Safe to call with external synchronization.
        /// </summary>
        void set_shininess(float value);
        /// <summary>
        /// Purpose: Returns the Blinn-Phong shininess exponent used by the material.
        /// Ownership: Returns a value copy; the caller owns the result.
        /// Thread Safety: Safe for concurrent reads.
        /// </summary>
        float get_shininess() const;
        /// <summary>
        /// Purpose: Sets the strength used when blending the shininess map with the base shininess.
        /// Ownership: Stores the value in the material parameter bindings.
        /// Thread Safety: Safe to call with external synchronization.
        /// </summary>
        void set_shininess_map_strength(float value);
        /// <summary>
        /// Purpose: Returns the strength used when blending the shininess map with the base shininess.
        /// Ownership: Returns a value copy; the caller owns the result.
        /// Thread Safety: Safe for concurrent reads.
        /// </summary>
        float get_shininess_map_strength() const;
        /// <summary>
        /// Purpose: Sets the emissive color contribution used by the material.
        /// Ownership: Stores the color by value in the material parameter bindings.
        /// Thread Safety: Safe to call with external synchronization.
        /// </summary>
        void set_emissive(Color value);
        /// <summary>
        /// Purpose: Returns the emissive color contribution used by the material.
        /// Ownership: Returns a value copy; the caller owns the result.
        /// Thread Safety: Safe for concurrent reads.
        /// </summary>
        Color get_emissive() const;
        /// <summary>
        /// Purpose: Sets the strength used when blending the emissive map with the base emissive.
        /// Ownership: Stores the value in the material parameter bindings.
        /// Thread Safety: Safe to call with external synchronization.
        /// </summary>
        void set_emissive_map_strength(float value);
        /// <summary>
        /// Purpose: Returns the strength used when blending the emissive map with the base emissive.
        /// Ownership: Returns a value copy; the caller owns the result.
        /// Thread Safety: Safe for concurrent reads.
        /// </summary>
        float get_emissive_map_strength() const;
        /// <summary>
        /// Purpose: Sets the ambient occlusion multiplier used by the material.
        /// Ownership: Stores the value in the material parameter bindings.
        /// Thread Safety: Safe to call with external synchronization.
        /// </summary>
        void set_occlusion(float value);
        /// <summary>
        /// Purpose: Returns the ambient occlusion multiplier used by the material.
        /// Ownership: Returns a value copy; the caller owns the result.
        /// Thread Safety: Safe for concurrent reads.
        /// </summary>
        float get_occlusion() const;
        /// <summary>
        /// Purpose: Sets the strength used when blending the occlusion map with the base occlusion.
        /// Ownership: Stores the value in the material parameter bindings.
        /// Thread Safety: Safe to call with external synchronization.
        /// </summary>
        void set_occlusion_map_strength(float value);
        /// <summary>
        /// Purpose: Returns the strength used when blending the occlusion map with the base occlusion.
        /// Ownership: Returns a value copy; the caller owns the result.
        /// Thread Safety: Safe for concurrent reads.
        /// </summary>
        float get_occlusion_map_strength() const;
        /// <summary>
        /// Purpose: Sets the alpha test threshold used by the material.
        /// Ownership: Stores the value in the material parameter bindings.
        /// Thread Safety: Safe to call with external synchronization.
        /// </summary>
        void set_alpha_cutoff(float value);
        /// <summary>
        /// Purpose: Returns the alpha test threshold used by the material.
        /// Ownership: Returns a value copy; the caller owns the result.
        /// Thread Safety: Safe for concurrent reads.
        /// </summary>
        float get_alpha_cutoff() const;
        /// <summary>
        /// Purpose: Sets the transparency blend amount used for forward transparent rendering.
        /// Ownership: Stores the value in the material parameter bindings.
        /// Thread Safety: Safe to call with external synchronization.
        /// </summary>
        void set_transparency_amount(float value);
        /// <summary>
        /// Purpose: Returns the transparency blend amount used for forward transparent rendering.
        /// Ownership: Returns a value copy; the caller owns the result.
        /// Thread Safety: Safe for concurrent reads.
        /// </summary>
        float get_transparency_amount() const;
        /// <summary>
        /// Purpose: Sets the exposure multiplier applied after lighting.
        /// Ownership: Stores the value in the material parameter bindings.
        /// Thread Safety: Safe to call with external synchronization.
        /// </summary>
        void set_exposure(float value);
        /// <summary>
        /// Purpose: Returns the exposure multiplier applied after lighting.
        /// Ownership: Returns a value copy; the caller owns the result.
        /// Thread Safety: Safe for concurrent reads.
        /// </summary>
        float get_exposure() const;
    };

    struct TBX_API Sky
    {
        MaterialInstance material = {};
    };
}
