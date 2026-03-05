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
            float metallic = 0.0f,
            float roughness = 1.0f,
            float occlusion = 1.0f,
            float alpha_cutoff = 0.1f,
            const Handle& diffuse = Handle(),
            const Handle& normal = Handle(),
            const Handle& material_handle = Handle());
        StandardMaterialInstance(const MaterialInstance& other);

        void set_diffuse(Handle value);
        Handle get_diffuse() const;
        void set_normal(Handle value);
        Handle get_normal() const;
        void set_color(Color value);
        Color get_color() const;
        void set_metallic(float value);
        float get_metallic() const;
        void set_roughness(float value);
        float get_roughness() const;
        void set_emissive(Color value);
        Color get_emissive() const;
        void set_occlusion(float value);
        float get_occlusion() const;
        void set_alpha_cutoff(float value);
        float get_alpha_cutoff() const;
        void set_exposure(float value);
        float get_exposure() const;
    };

    struct TBX_API Sky
    {
        MaterialInstance material = {};
    };
}
