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
#include <utility>
#include <vector>

namespace tbx
{
    /// <summary>
    /// Purpose: Represents the supported values for material parameters.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns any stored value data.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.
    /// </remarks>
    using MaterialParameterData =
        std::variant<bool, int, float, double, Vec2, Vec3, Vec4, RgbaColor, Mat3, Mat4>;

    /// <summary>
    /// Purpose: Backward-compatible alias for material parameter value payloads.
    /// </summary>
    /// <remarks>
    /// Ownership: Alias only; ownership semantics match MaterialParameterData.
    /// Thread Safety: Alias only; thread safety semantics match MaterialParameterData.
    /// </remarks>
    using UniformData = MaterialParameterData;

    /// <summary>
    /// Purpose: Represents a uniform upload payload for shader programs.
    /// </summary>
    /// <remarks>
    /// Ownership: Stores owned copies of uniform data values.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.
    /// </remarks>
    struct TBX_API MaterialParameter
    {
        std::string name = "";
        MaterialParameterData data = 0;
    };

    /// <summary>
    /// Purpose: Stores a material parameter binding name and value.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns the binding name and parameter value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    /// </remarks>
    struct TBX_API MaterialParameterBinding
    {
        std::string name = {};
        MaterialParameterData value = 0.0f;
    };

    /// <summary>
    /// Purpose: Stores a mutable list of named material parameters.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns parameter names and values.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.
    /// </remarks>
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

    /// <summary>
    /// Purpose: Stores a runtime texture binding name and texture data.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns the binding name and TextureInstance value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    /// </remarks>
    struct TBX_API MaterialTextureBinding
    {
        std::string name = {};
        TextureInstance texture = {};
    };

    /// <summary>
    /// Purpose: Stores per-material texture values applied at render time.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns uniform-name strings and runtime texture values.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.
    /// </remarks>
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

    /// <summary>
    /// Purpose: Defines a mutable collection of material parameters.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns its parameter collections.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.
    /// </remarks>
    struct TBX_API Material
    {
        /// <summary>
        /// Purpose: Provides explicit shader stage handles used to build a single shader program.
        /// </summary>
        /// <remarks>
        /// Ownership: Stores stage handles by value; does not own loaded shader assets.
        /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
        /// </remarks>
        ShaderProgram program = {};

        /// <summary>
        /// Purpose: Stores named texture asset bindings for the material.
        /// </summary>
        /// <remarks>
        /// Ownership: Owns the texture name strings and handle values.
        /// Thread Safety: Safe for concurrent reads; synchronize concurrent mutation externally.
        /// </remarks>
        MaterialTextureBindings textures = {};

        /// <summary>
        /// Purpose: Stores material parameters.
        /// </summary>
        /// <remarks>
        /// Ownership: Owns the parameter list and associated values.
        /// Thread Safety: Safe for concurrent reads; synchronize concurrent mutation externally.
        /// </remarks>
        MaterialParameterBindings parameters = {};
    };

    /// <summary>
    /// Purpose: Describes runtime material data used by renderers and post-processing effects.
    /// </summary>
    /// <remarks>
    /// Ownership: Stores a non-owning base material handle plus owned parameter/texture override
    /// sets. Shader stages are always sourced from the base material and are not runtime
    /// overrideable.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.
    /// </remarks>
    struct TBX_API MaterialInstance
    {
        /// <summary>
        /// Purpose: Base material asset used to resolve shader stages and default bindings.
        /// </summary>
        /// <remarks>
        /// Ownership: Stores a non-owning material handle reference.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        /// </remarks>
        Handle handle = {};

        /// <summary>
        /// Purpose: Runtime parameter values layered onto the base material.
        /// </summary>
        /// <remarks>
        /// Ownership: Owns override values by name.
        /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
        /// </remarks>
        MaterialParameterBindings parameters = {};

        /// <summary>
        /// Purpose: Runtime texture values layered onto the base material.
        /// </summary>
        /// <remarks>
        /// Ownership: Owns override sampler-name/handle pairs.
        /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
        /// </remarks>
        MaterialTextureBindings textures = {};
    };

    /// <summary>
    /// Purpose: Defines the active sky material used for skybox rendering and clear-color
    /// fallback.
    /// </summary>
    /// <remarks>
    /// Ownership: Stores a non-owning material handle reference.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.
    /// </remarks>
    struct TBX_API Sky
    {
        /// <summary>
        /// Purpose: Runtime material data used by the renderer to resolve sky shading data.
        /// </summary>
        /// <remarks>
        /// Ownership: Owns parameter/texture override sets and a non-owning base material
        /// handle.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        /// </remarks>
        MaterialInstance material = {};
    };
}
