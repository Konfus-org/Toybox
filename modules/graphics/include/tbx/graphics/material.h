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
    };

    struct TBX_API Sky
    {
        MaterialInstance material = {};
    };
}
