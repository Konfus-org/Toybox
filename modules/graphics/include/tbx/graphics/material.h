#pragma once
#include "tbx/common/handle.h"
#include "tbx/graphics/color.h"
#include "tbx/math/vectors.h"
#include "tbx/tbx_api.h"
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>

namespace tbx
{
    /// <summary>
    /// Purpose: Represents the supported values for material parameters.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns any stored value data, including asset handles.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.
    /// </remarks>
    using MaterialParameterValue =
        std::variant<bool, int, float, Vec2, Vec3, Vec4, RgbaColor, Handle>;

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
    /// Purpose: Defines a mutable collection of material parameters.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns its parameter map and associated values.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.
    /// </remarks>
    struct TBX_API Material
    {
        Material() = default;

        /// <summary>
        /// Purpose: Creates a material with a provided parameter map.
        /// </summary>
        /// <remarks>
        /// Ownership: Takes ownership of the provided parameter data.
        /// Thread Safety: Safe to construct on any thread.
        /// </remarks>
        explicit Material(std::unordered_map<std::string, MaterialParameterValue> material_params)
            : parameters(std::move(material_params))
        {
        }

        /// <summary>
        /// Purpose: Assigns a parameter value by name.
        /// </summary>
        /// <remarks>
        /// Ownership: Stores a copy of the provided value in the parameter map.
        /// Thread Safety: Not thread-safe; synchronize concurrent mutation externally.
        /// </remarks>
        void set_parameter(std::string name, MaterialParameterValue value)
        {
            parameters.insert_or_assign(std::move(name), std::move(value));
        }

        /// <summary>
        /// Purpose: Assigns a parameter value by name using a typed value.
        /// </summary>
        /// <remarks>
        /// Ownership: Stores a copy of the provided value in the parameter map.
        /// Thread Safety: Not thread-safe; synchronize concurrent mutation externally.
        /// </remarks>
        template <typename TValue>
        void set_parameter(const std::string& name, TValue value)
        {
            MaterialParameterValue parameter_value = std::move(value);
            set_parameter(std::string(name), std::move(parameter_value));
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
            auto iterator = parameters.find(std::string(name));
            if (iterator == parameters.end())
            {
                return nullptr;
            }
            return &iterator->second;
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
            auto iterator = parameters.find(std::string(name));
            if (iterator == parameters.end())
            {
                return nullptr;
            }
            return &iterator->second;
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
        /// Purpose: Stores material parameters. Defaults to a standard PBR parameter set.
        /// </summary>
        /// <remarks>
        /// Ownership: Owns the parameter map and associated values.
        /// Thread Safety: Safe for concurrent reads; synchronize concurrent mutation externally.
        /// </remarks>
        std::unordered_map<std::string, MaterialParameterValue> parameters = {
            {"Shader", default_shader_handle},
            {"Color", RgbaColor(1.0f, 1.0f, 1.0f, 1.0f)},
            {"Diffuse", Handle()},
            {"Normal", Handle()},
            {"Metallic", 0.0f},
            {"Roughness", 1.0f},
            {"Emissive", RgbaColor(0.0f, 0.0f, 0.0f, 1.0f)},
            {"Occlusion", 1.0f},
        };
    };
}
