#pragma once
#include "tbx/common/handle.h"
#include "tbx/graphics/color.h"
#include "tbx/graphics/shader.h"
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
    /// Purpose: Stores a named texture binding for a material.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns the parameter name string and handle value.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.
    /// </remarks>
    struct TBX_API MaterialTexture
    {
        std::string name;
        Handle handle;
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
        /// Purpose: Identifies the shader assets used to render the material.
        /// </summary>
        /// <remarks>
        /// Ownership: Stores the shader handles by value.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        /// </remarks>
        std::vector<Handle> shaders = {};

        /// <summary>
        /// Purpose: Stores named texture bindings for the material.
        /// </summary>
        /// <remarks>
        /// Ownership: Owns the texture list and handles by value.
        /// Thread Safety: Safe for concurrent reads; synchronize concurrent mutation externally.
        /// </remarks>
        std::vector<MaterialTexture> textures = {};

        /// <summary>
        /// Purpose: Stores material parameters.
        /// </summary>
        /// <remarks>
        /// Ownership: Owns the parameter list and associated values.
        /// Thread Safety: Safe for concurrent reads; synchronize concurrent mutation externally.
        /// </remarks>
        std::vector<ShaderUniform> parameters = {};
    };
}
