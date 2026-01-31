#pragma once
#include "tbx/common/handle.h"
#include "tbx/graphics/color.h"
#include "tbx/math/vectors.h"
#include "tbx/tbx_api.h"
#include <string>
#include <string_view>
#include <utility>
#include <variant>
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
    using MaterialParameterValue = std::variant<bool, int, float, Vec2, Vec3, Vec4, RgbaColor>;

    /// <summary>
    /// Purpose: Provides the default shader asset handle.
    /// </summary>
    /// <remarks>
    /// Ownership: Returns a value handle; no ownership transfer.
    /// Thread Safety: Safe to read concurrently.
    /// </remarks>
    inline const Handle default_shader_handle = Handle(Uuid(0x1U));

    /// <summary>
    /// Purpose: Provides the default material asset handle.
    /// </summary>
    /// <remarks>
    /// Ownership: Returns a value handle; no ownership transfer.
    /// Thread Safety: Safe to read concurrently.
    /// </remarks>
    inline const Handle default_material_handle = Handle(Uuid(0x2U));

    /// <summary>
    /// Purpose: Stores a named material parameter value.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns the parameter name string and value data.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.
    /// </remarks>
    struct TBX_API MaterialParameter
    {
        std::string name = "";
        MaterialParameterValue value = 0.0f;
    };

    /// <summary>
    /// Purpose: Stores a named texture binding for a material.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns the parameter name string and handle value.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.
    /// </remarks>
    struct TBX_API MaterialTexture
    {
        std::string name = "";
        Handle handle = {};
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
        Material() = default;

        /// <summary>
        /// Purpose: Creates a material with a provided parameter list.
        /// </summary>
        /// <remarks>
        /// Ownership: Takes ownership of the provided parameter data.
        /// Thread Safety: Safe to construct on any thread.
        /// </remarks>
        explicit Material(std::vector<MaterialParameter> material_params)
            : parameters(std::move(material_params))
        {
        }

        /// <summary>
        /// Purpose: Assigns a parameter value by name.
        /// </summary>
        /// <remarks>
        /// Ownership: Stores a copy of the provided value in the parameter list.
        /// Thread Safety: Not thread-safe; synchronize concurrent mutation externally.
        /// </remarks>
        void set_parameter(std::string name, MaterialParameterValue value)
        {
            for (auto& parameter : parameters)
            {
                if (parameter.name == name)
                {
                    parameter.value = std::move(value);
                    return;
                }
            }

            MaterialParameter parameter = {};
            parameter.name = std::move(name);
            parameter.value = std::move(value);
            parameters.push_back(std::move(parameter));
        }

        /// <summary>
        /// Purpose: Assigns a parameter value by name using a typed value.
        /// </summary>
        /// <remarks>
        /// Ownership: Stores a copy of the provided value in the parameter list.
        /// Thread Safety: Not thread-safe; synchronize concurrent mutation externally.
        /// </remarks>
        template <typename TValue>
        void set_parameter(const std::string& name, TValue value)
        {
            MaterialParameterValue parameter_value = std::move(value);
            set_parameter(std::string(name), std::move(parameter_value));
        }

        /// <summary>
        /// Purpose: Assigns a texture handle by name.
        /// </summary>
        /// <remarks>
        /// Ownership: Stores a copy of the provided handle.
        /// Thread Safety: Not thread-safe; synchronize concurrent mutation externally.
        /// </remarks>
        void set_texture(std::string name, Handle handle)
        {
            for (auto& texture : textures)
            {
                if (texture.name == name)
                {
                    texture.handle = std::move(handle);
                    return;
                }
            }

            MaterialTexture texture = {};
            texture.name = std::move(name);
            texture.handle = std::move(handle);
            textures.push_back(std::move(texture));
        }

        /// <summary>
        /// Purpose: Retrieves a texture handle pointer by name.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not transfer ownership; returned pointer is owned by the material.
        /// Thread Safety: Safe for concurrent reads; synchronize concurrent mutation externally.
        /// </remarks>
        const Handle* get_texture(std::string_view name) const
        {
            for (const auto& texture : textures)
            {
                if (texture.name == name)
                {
                    return &texture.handle;
                }
            }
            return nullptr;
        }

        /// <summary>
        /// Purpose: Retrieves a mutable texture handle pointer by name.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not transfer ownership; returned pointer is owned by the material.
        /// Thread Safety: Not thread-safe; synchronize concurrent mutation externally.
        /// </remarks>
        Handle* get_texture(std::string_view name)
        {
            for (auto& texture : textures)
            {
                if (texture.name == name)
                {
                    return &texture.handle;
                }
            }
            return nullptr;
        }

        /// <summary>
        /// Purpose: Retrieves a parameter value pointer by name.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not transfer ownership; returned pointer is owned by the material.
        /// Thread Safety: Safe for concurrent reads; synchronize concurrent mutation externally.
        /// </remarks>
        const MaterialParameterValue* get_parameter(std::string_view name) const
        {
            for (const auto& parameter : parameters)
            {
                if (parameter.name == name)
                {
                    return &parameter.value;
                }
            }
            return nullptr;
        }

        /// <summary>
        /// Purpose: Retrieves a mutable parameter value pointer by name.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not transfer ownership; returned pointer is owned by the material.
        /// Thread Safety: Not thread-safe; synchronize concurrent mutation externally.
        /// </remarks>
        MaterialParameterValue* get_parameter(std::string_view name)
        {
            for (auto& parameter : parameters)
            {
                if (parameter.name == name)
                {
                    return &parameter.value;
                }
            }
            return nullptr;
        }

        /// <summary>
        /// Purpose: Tries to retrieve a typed parameter by name.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not transfer ownership; outputs a copy of the stored value.
        /// Thread Safety: Safe for concurrent reads; synchronize concurrent mutation externally.
        /// </remarks>
        template <typename TValue>
        bool try_get_parameter(std::string_view name, TValue& out_value) const
        {
            const MaterialParameterValue* value = get_parameter(name);
            if (!value)
            {
                return false;
            }
            const TValue* typed_value = std::get_if<TValue>(value);
            if (!typed_value)
            {
                return false;
            }
            out_value = *typed_value;
            return true;
        }

        /// <summary>
        /// Purpose: Identifies the shader assets used to render the material.
        /// </summary>
        /// <remarks>
        /// Ownership: Stores the shader handles by value.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        /// </remarks>
        std::vector<Handle> shaders = {default_shader_handle};

        /// <summary>
        /// Purpose: Stores named texture bindings for the material.
        /// </summary>
        /// <remarks>
        /// Ownership: Owns the texture list and handles by value.
        /// Thread Safety: Safe for concurrent reads; synchronize concurrent mutation externally.
        /// </remarks>
        std::vector<MaterialTexture> textures = {
            {"diffuse", Handle()},
            {"normal", Handle()},
        };

        /// <summary>
        /// Purpose: Stores material parameters. Defaults to a standard PBR parameter set.
        /// </summary>
        /// <remarks>
        /// Ownership: Owns the parameter list and associated values.
        /// Thread Safety: Safe for concurrent reads; synchronize concurrent mutation externally.
        /// </remarks>
        std::vector<MaterialParameter> parameters = {
            {"color", RgbaColor(1.0f, 1.0f, 1.0f, 1.0f)},
            {"metallic", 0.0f},
            {"roughness", 1.0f},
            {"emissive", RgbaColor(0.0f, 0.0f, 0.0f, 1.0f)},
            {"occlusion", 1.0f},
        };
    };
}
